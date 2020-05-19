//TODO: write timing code that doesn't rely on instrumentation inside giff.c

#include "../msf_gif.h"
#include "trace.hpp"
#include "List.hpp"
#include "common.hpp"

#include "gif.h"
#include "jo_gif.cpp"

struct RawBlob {
    int width, height, frames, centiSeconds;
    int pixels[1];
};

// #include <unistd.h>

int main() {
    // printf("logical cores available: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    const char * names[] = {
        "bouncy", "diwide-large", "diwide", "floor", "increase", "keyhole", "odd", "tiles",
        "anchor", "always-in-front", "flip",
        // "sky",
        // "keyhole",
    };
    // const char * names[] = { "bouncy", "diwide", "increase" };

    List<RawBlob *> blobs = {};
    for (const char * name : names) {
        char * path = dsprintf(nullptr, "in/%s.rawframes", name);
        printf("loading %s...\n", path); fflush(stdout);
        blobs.add((RawBlob *) read_entire_file(path));
    }

    struct TimerInfo {
        double time;
        size_t in, out;
    };
    List<TimerInfo> timers = {};
    init_profiling_trace();

    //TODO: write both dithered and non-dithered versions each to their own folder?
    //TODO: automatically regression-test against known good versions of the GIFs?
#if 1
    for (int i = 0; i < blobs.len; ++i) {
        RawBlob * blob = blobs[i];
        bool flipped = blob->height < 0;
        if (flipped) blob->height *= -1;

        List<uint8_t *> frames = {};
        for (int j = 0; j < blob->frames; ++j) {
            frames.add((u8 *) &blob->pixels[blob->width * blob->height * j]);
        }

        //write gifs using the new algorithm
        char * path = dsprintf(nullptr, "new/%s.gif", names[i]);
        TimeScope(path);
        printf("writing %24s      width: %d   height: %d   frames: %d   centiSeconds: %d\n",
            path, blob->width, blob->height, blob->frames, blob->centiSeconds); fflush(stdout);
        double pre = get_time();
        size_t out;
        out = msf_gif_save(path, frames.data, frames.len, blob->width, blob->height, 15, blob->centiSeconds, flipped, 8);
        timers.add({ get_time() - pre, (size_t)(blob->frames * blob->width * blob->height * 4), out });
    }
#else
    for (int i = 0; i < blobs.len; ++i) {
        RawBlob * blob = blobs[i];
        bool flipped = blob->height < 0;
        if (flipped) blob->height *= -1;

        List<uint8_t *> frames = {};
        for (int j = 0; j < blob->frames; ++j) {
            frames.add((u8 *) &blob->pixels[blob->width * blob->height * j]);
        }

        //since this exporter doesn support direct export of flipped frame data, we have to flip them back manually
        if (flipped) {
            for (uint8_t * frame : frames) {
                for (int y = 0; y < blob->height / 2; ++y) {
                    for (int x = 0; x < blob->width * 4; ++x) {
                        swap(frame[y * blob->width * 4 + x], frame[(blob->height - y - 1) * blob->width * 4 + x]);
                    }
                }
            }
        }

    #if 1
        char * path = dsprintf(nullptr, "gif-h3/%s.gif", names[i]);
        TimeScope(path);
        printf("writing %28s      width: %d   height: %d   frames: %d   centiSeconds: %d\n",
            path, blob->width, blob->height, blob->frames, blob->centiSeconds); fflush(stdout);
        double pre = get_time();

        GifWriter writer = {};
        GifBegin(&writer, path, blob->width, blob->height, blob->centiSeconds, 7, true);
        for (uint8_t * frame : frames) {
            GifWriteFrame(&writer, frame, blob->width, blob->height, blob->centiSeconds, 7, true);
        }
        GifEnd(&writer);
    #else
        char * path = dsprintf(nullptr, "jo_gif2/%s.gif", names[i]);
        TimeScope(path);
        printf("writing %24s      width: %d   height: %d   frames: %d   centiSeconds: %d\n",
            path, blob->width, blob->height, blob->frames, blob->centiSeconds); fflush(stdout);
        double pre = get_time();

        jo_gif_t writer = jo_gif_start(path, blob->width, blob->height, 0, 255);
        for (uint8_t * frame : frames) {
            jo_gif_frame(&writer, frame, blob->centiSeconds, true);
        }
        jo_gif_end(&writer);
    #endif
        FILE * fp = fopen(path, "rb");
        assert(fp);
        fseek(fp, 0, SEEK_END);
        size_t out = ftell(fp);
        fclose(fp);
        timers.add({ get_time() - pre, (size_t)(blob->frames * blob->width * blob->height * 4), out });
    }
#endif

    printf("\n");
    TimerInfo totals = {};
    for (int i = 0; i < timers.len; ++i) {
        totals.time += timers[i].time;
        totals.in += timers[i].in;
        totals.out += timers[i].out;
        printf("%16s      time: %6.3f   in: %4.2f GB/s   out: %5.2f MB/s\n",
            names[i], timers[i].time,
            timers[i].in / timers[i].time / 1024 / 1024 / 1024, timers[i].out / timers[i].time / 1024 / 1024);
    }
    printf("\n%16s      time: %6.3f   in: %4.2f GB/s   out: %5.2f MB/s\n",
        "totals", totals.time, totals.in / totals.time / 1024 / 1024 / 1024, totals.out / totals.time / 1024 / 1024);

    trace_end_event("main");
    print_profiling_trace();
    return 0;
}