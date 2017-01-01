/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "Benchmark.h"
#include "MockMediaResource.h"
#include "DecoderTraits.h"
#include "MediaContentType.h"
#include "MP4Decoder.h"
#include "MP4Demuxer.h"
#include "WebMDecoder.h"
#include "WebMDemuxer.h"

using namespace mozilla;

class BenchmarkRunner
{
public:
  explicit BenchmarkRunner(Benchmark* aBenchmark)
    : mBenchmark(aBenchmark) {}

  uint32_t Run()
  {
    bool done = false;
    uint32_t result = 0;

    mBenchmark->Init();
    mBenchmark->Run()->Then(
      AbstractThread::MainThread(), __func__,
      [&](uint32_t aDecodeFps) { result = aDecodeFps; done = true; },
      [&]() { done = true; });

    // Wait until benchmark completes.
    while (!done) {
      NS_ProcessNextEvent();
    }
    return result;
  }

private:
  RefPtr<Benchmark> mBenchmark;
};

TEST(MediaDataDecoder, H264)
{
  if (!DecoderTraits::IsMP4SupportedType(
         MediaContentType(MEDIAMIMETYPE("video/mp4")),
         /* DecoderDoctorDiagnostics* */ nullptr)) {
    EXPECT_TRUE(true);
  } else {
    RefPtr<MediaResource> resource =
      new MockMediaResource("gizmo.mp4", NS_LITERAL_CSTRING("video/mp4"));
    nsresult rv = resource->Open(nullptr);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    BenchmarkRunner runner(new Benchmark(new MP4Demuxer(resource)));
    EXPECT_GT(runner.Run(), 0u);
  }
}

TEST(MediaDataDecoder, VP9)
{
  if (!WebMDecoder::IsSupportedType(MediaContentType(MEDIAMIMETYPE("video/webm")))) {
    EXPECT_TRUE(true);
  } else {
    RefPtr<MediaResource> resource =
      new MockMediaResource("vp9cake.webm", NS_LITERAL_CSTRING("video/webm"));
    nsresult rv = resource->Open(nullptr);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    BenchmarkRunner runner(new Benchmark(new WebMDemuxer(resource)));
    EXPECT_GT(runner.Run(), 0u);
  }
}
