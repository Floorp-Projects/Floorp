/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorBench.h"

#ifdef MOZ_COMPOSITOR_BENCH
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/TimeStamp.h"
#include "gfxColor.h"
#include "gfxPrefs.h"
#include <math.h>
#include "GeckoProfiler.h"

#define TEST_STEPS 1000
#define DURATION_THRESHOLD 30
#define THRESHOLD_ABORT_COUNT 5

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

static float SimplePseudoRandom(int aStep, int aCount) {
  srand(aStep * 1000 + aCount);
  return static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}

class BenchTest {
public:
  BenchTest(const char* aTestName)
    : mTestName(aTestName)
  {}

  virtual ~BenchTest() {}

  virtual void Setup(Compositor* aCompositor, size_t aStep) {}
  virtual void Teardown(Compositor* aCompositor) {}
  virtual void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) = 0;

  const char* ToString() { return mTestName; }
private:
  const char* mTestName;
};

class EffectSolidColorBench : public BenchTest {
public:
  EffectSolidColorBench()
    : BenchTest("EffectSolidColorBench (clear frame with EffectSolidColor)")
  {}

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
    float red;
    float tmp;
    red = modf(aStep * 0.03f, &tmp);
    EffectChain effects;
    gfxRGBA color(red, 0.4f, 0.4f, 1.0f);
    effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(color.r,
                                                             color.g,
                                                             color.b,
                                                             color.a));

    const gfx::Rect& rect = aScreenRect;
    const gfx::Rect& clipRect = aScreenRect;

    float opacity = 1.f;

    gfx::Matrix4x4 transform;
    aCompositor->DrawQuad(rect, clipRect, effects, opacity, transform);
  }
};

class EffectSolidColorTrivialBench : public BenchTest {
public:
  EffectSolidColorTrivialBench()
    : BenchTest("EffectSolidColorTrivialBench (10s 1x1 EffectSolidColor)")
  {}

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
    for (size_t i = 0; i < aStep * 10; i++) {
      float red;
      float tmp;
      red = modf(i * 0.03f, &tmp);
      EffectChain effects;
      gfxRGBA color(red, 0.4f, 0.4f, 1.0f);
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(color.r,
                                                               color.g,
                                                               color.b,
                                                               color.a));

      const gfx::Rect& rect = gfx::Rect(i % (int)aScreenRect.width,
                                        (int)(i / aScreenRect.height),
                                        1, 1);
      const gfx::Rect& clipRect = aScreenRect;

      float opacity = 1.f;

      gfx::Matrix transform2d;

      gfx::Matrix4x4 transform = gfx::Matrix4x4::From2D(transform2d);

      aCompositor->DrawQuad(rect, clipRect, effects, opacity, transform);
    }
  }
};

class EffectSolidColorStressBench : public BenchTest {
public:
  EffectSolidColorStressBench()
    : BenchTest("EffectSolidColorStressBench (10s various EffectSolidColor)")
  {}

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
    for (size_t i = 0; i < aStep * 10; i++) {
      float red;
      float tmp;
      red = modf(i * 0.03f, &tmp);
      EffectChain effects;
      gfxRGBA color(red, 0.4f, 0.4f, 1.0f);
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(color.r,
                                                               color.g,
                                                               color.b,
                                                               color.a));

      const gfx::Rect& rect = gfx::Rect(aScreenRect.width * SimplePseudoRandom(i, 0),
                                        aScreenRect.height * SimplePseudoRandom(i, 1),
                                        aScreenRect.width * SimplePseudoRandom(i, 2),
                                        aScreenRect.height * SimplePseudoRandom(i, 3));
      const gfx::Rect& clipRect = aScreenRect;

      float opacity = 0.3f;

      gfx::Matrix transform2d;
      transform2d = transform2d.Rotate(SimplePseudoRandom(i, 4) * 70.f);

      gfx::Matrix4x4 transform = gfx::Matrix4x4::From2D(transform2d);

      aCompositor->DrawQuad(rect, clipRect, effects, opacity, transform);
    }
  }
};

class UploadBench : public BenchTest {
public:
  UploadBench()
    : BenchTest("Upload Bench (10s 256x256 upload)")
  {}

  uint32_t* mBuf;
  RefPtr<DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    int bytesPerPixel = 4;
    int w = 256;
    int h = 256;
    mBuf = (uint32_t *) malloc(w * h * sizeof(uint32_t));

    mSurface = Factory::CreateWrappingDataSourceSurface(
      reinterpret_cast<uint8_t*>(mBuf), w * bytesPerPixel, IntSize(w, h), SurfaceFormat::B8G8R8A8);
    mTexture = aCompositor->CreateDataTextureSource();
  }

  virtual void Teardown(Compositor* aCompositor) {
    mSurface = nullptr;
    mTexture = nullptr;
    free(mBuf);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
    for (size_t i = 0; i < aStep * 10; i++) {
      mTexture->Update(mSurface);
    }
  }
};

static void RunCompositorBench(Compositor* aCompositor, const gfx::Rect& aScreenRect)
{
  std::vector<BenchTest*> tests;

  tests.push_back(new EffectSolidColorBench());
  tests.push_back(new EffectSolidColorTrivialBench());
  tests.push_back(new EffectSolidColorStressBench());
  tests.push_back(new UploadBench());

  for (size_t i = 0; i < tests.size(); i++) {
    BenchTest* test = tests[i];
    std::vector<TimeDuration> results;
    int testsOverThreshold = 0;
    PROFILER_MARKER(test->ToString());
    for (size_t j = 0; j < TEST_STEPS; j++) {
      test->Setup(aCompositor, j);

      TimeStamp start = TimeStamp::Now();
      nsIntRect screenRect(aScreenRect.x, aScreenRect.y,
                           aScreenRect.width, aScreenRect.height);
      aCompositor->BeginFrame(
        nsIntRect(screenRect.x, screenRect.y,
                  screenRect.width, screenRect.height),
        nullptr, gfx::Matrix(), aScreenRect);

      test->DrawFrame(aCompositor, aScreenRect, j);

      aCompositor->EndFrame();
      results.push_back(TimeStamp::Now() - start);

      if (results[j].ToMilliseconds() > DURATION_THRESHOLD) {
        testsOverThreshold++;
        if (testsOverThreshold == THRESHOLD_ABORT_COUNT) {
          test->Teardown(aCompositor);
          break;
        }
      } else {
        testsOverThreshold = 0;
      }
      test->Teardown(aCompositor);
    }

    printf_stderr("%s\n", test->ToString());
    printf_stderr("Run step, Time (ms)\n");
    for (size_t j = 0; j < results.size(); j++) {
      printf_stderr("%i,%f\n", j, (float)results[j].ToMilliseconds());
    }
    printf_stderr("\n", test->ToString());
  }

  for (size_t i = 0; i < tests.size(); i++) {
    delete tests[i];
  }
}

void CompositorBench(Compositor* aCompositor, const gfx::Rect& aScreenRect)
{
  static bool sRanBenchmark = false;
  bool wantBenchmark = gfxPrefs::LayersBenchEnabled();
  if (wantBenchmark && !sRanBenchmark) {
    RunCompositorBench(aCompositor, aScreenRect);
  }
  sRanBenchmark = wantBenchmark;
}

}
}
#endif

