/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ADTSDemuxer.h"
#include "Benchmark.h"
#include "BufferMediaResource.h"
#include "FlacDemuxer.h"
#include "FuzzingInterface.h"
#include "mozilla/AbstractThread.h"
#include "MP3Demuxer.h"
#include "MP4Demuxer.h"
#include "OggDemuxer.h"
#include "systemservices/MediaUtils.h"
#include "WaveDemuxer.h"
#include "WebMDemuxer.h"

using namespace mozilla;

class FuzzRunner {
 public:
  explicit FuzzRunner(Benchmark* aBenchmark) : mBenchmark(aBenchmark) {}

  void Run() {
    mBenchmark->Init();
    media::Await(
        GetMediaThreadPool(MediaThreadType::PLAYBACK), mBenchmark->Run(),
        [&](uint32_t aDecodeFps) {}, [&](const MediaResult& aError) {});
    return;
  }

 private:
  RefPtr<Benchmark> mBenchmark;
};

static int FuzzingInitMedia(int* argc, char*** argv) {
  /* Generic no-op initialization used for all targets */
  return 0;
}

#define MOZ_MEDIA_FUZZER(_name)                                         \
  static int FuzzingRunMedia##_name(const uint8_t* data, size_t size) { \
    if (!size) {                                                        \
      return 0;                                                         \
    }                                                                   \
    RefPtr<BufferMediaResource> resource =                              \
        new BufferMediaResource(data, size);                            \
    FuzzRunner runner(new Benchmark(new _name##Demuxer(resource)));     \
    runner.Run();                                                       \
    return 0;                                                           \
  }                                                                     \
  MOZ_FUZZING_INTERFACE_RAW(FuzzingInitMedia, FuzzingRunMedia##_name,   \
                            Media##_name);

MOZ_MEDIA_FUZZER(ADTS);
MOZ_MEDIA_FUZZER(Flac);
MOZ_MEDIA_FUZZER(MP3);
MOZ_MEDIA_FUZZER(MP4);
MOZ_MEDIA_FUZZER(Ogg);
MOZ_MEDIA_FUZZER(WAV);
MOZ_MEDIA_FUZZER(WebM);
