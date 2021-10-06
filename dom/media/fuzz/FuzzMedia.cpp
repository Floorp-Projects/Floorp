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
#include "mozilla/SpinEventLoopUntil.h"
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
    // Assert we're on the main thread, otherwise `done` must be synchronized.
    MOZ_ASSERT(NS_IsMainThread());
    bool done = false;

    mBenchmark->Init();
    mBenchmark->Run()->Then(
        // Non DocGroup-version of AbstractThread::MainThread() is fine for
        // testing.
        AbstractThread::MainThread(), __func__,
        [&](uint32_t aDecodeFps) { done = true; }, [&]() { done = true; });

    // Wait until benchmark completes.
    SpinEventLoopUntil("FuzzRunner::Run"_ns, [&]() { return done; });
    return;
  }

 private:
  RefPtr<Benchmark> mBenchmark;
};

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
  MOZ_FUZZING_INTERFACE_RAW(nullptr, FuzzingRunMedia##_name, Media##_name);

MOZ_MEDIA_FUZZER(ADTS);
MOZ_MEDIA_FUZZER(Flac);
MOZ_MEDIA_FUZZER(MP3);
MOZ_MEDIA_FUZZER(MP4);
MOZ_MEDIA_FUZZER(Ogg);
MOZ_MEDIA_FUZZER(WAV);
MOZ_MEDIA_FUZZER(WebM);
