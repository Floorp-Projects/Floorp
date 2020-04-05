/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorBench.h"

#ifdef MOZ_COMPOSITOR_BENCH
#  include "mozilla/gfx/2D.h"
#  include "mozilla/layers/Compositor.h"
#  include "mozilla/layers/Effects.h"
#  include "mozilla/StaticPrefs_layers.h"
#  include "mozilla/TimeStamp.h"
#  include <math.h>
#  include "GeckoProfiler.h"

#  define TEST_STEPS 1000
#  define DURATION_THRESHOLD 30
#  define THRESHOLD_ABORT_COUNT 5

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

static float SimplePseudoRandom(int aStep, int aCount) {
  srand(aStep * 1000 + aCount);
  return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

class BenchTest {
 public:
  BenchTest(const char* aTestName) : mTestName(aTestName) {}

  virtual ~BenchTest() = default;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {}
  virtual void Teardown(Compositor* aCompositor) {}
  virtual void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect,
                         size_t aStep) = 0;

  const char* ToString() { return mTestName; }

 private:
  const char* mTestName;
};

static void DrawFrameTrivialQuad(Compositor* aCompositor,
                                 const gfx::Rect& aScreenRect, size_t aStep,
                                 const EffectChain& effects) {
  for (size_t i = 0; i < aStep * 10; i++) {
    const gfx::Rect& rect = gfx::Rect(i % (int)aScreenRect.width,
                                      (int)(i / aScreenRect.height), 1, 1);
    const gfx::Rect& clipRect = aScreenRect;

    float opacity = 1.f;

    gfx::Matrix transform2d;

    gfx::Matrix4x4 transform = gfx::Matrix4x4::From2D(transform2d);

    aCompositor->DrawQuad(rect, clipRect, effects, opacity, transform);
  }
}

static void DrawFrameStressQuad(Compositor* aCompositor,
                                const gfx::Rect& aScreenRect, size_t aStep,
                                const EffectChain& effects) {
  for (size_t i = 0; i < aStep * 10; i++) {
    const gfx::Rect& rect =
        gfx::Rect(aScreenRect.width * SimplePseudoRandom(i, 0),
                  aScreenRect.height * SimplePseudoRandom(i, 1),
                  aScreenRect.width * SimplePseudoRandom(i, 2),
                  aScreenRect.height * SimplePseudoRandom(i, 3));
    const gfx::Rect& clipRect = aScreenRect;

    float opacity = 1.f;

    gfx::Matrix transform2d;
    transform2d = transform2d.PreRotate(SimplePseudoRandom(i, 4) * 70.f);

    gfx::Matrix4x4 transform = gfx::Matrix4x4::From2D(transform2d);

    aCompositor->DrawQuad(rect, clipRect, effects, opacity, transform);
  }
}

class EffectSolidColorBench : public BenchTest {
 public:
  EffectSolidColorBench()
      : BenchTest("EffectSolidColorBench (clear frame with EffectSolidColor)") {
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect,
                 size_t aStep) {
    float tmp;
    float red = modff(aStep * 0.03f, &tmp);
    EffectChain effects;
    effects.mPrimaryEffect =
        new EffectSolidColor(gfx::DeviceColor(red, 0.4f, 0.4f, 1.0f));

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
      : BenchTest("EffectSolidColorTrivialBench (10s 1x1 EffectSolidColor)") {}

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect,
                 size_t aStep) {
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameTrivialQuad(aCompositor, aScreenRect, aStep, effects);
  }

  already_AddRefed<Effect> CreateEffect(size_t i) {
    float tmp;
    float red = modff(i * 0.03f, &tmp);
    EffectChain effects;
    return MakeAndAddRef<EffectSolidColor>(
        gfx::DeviceColor(red, 0.4f, 0.4f, 1.0f));
  }
};

class EffectSolidColorStressBench : public BenchTest {
 public:
  EffectSolidColorStressBench()
      : BenchTest(
            "EffectSolidColorStressBench (10s various EffectSolidColor)") {}

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect,
                 size_t aStep) {
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameStressQuad(aCompositor, aScreenRect, aStep, effects);
  }

  already_AddRefed<Effect> CreateEffect(size_t i) {
    float tmp;
    float red = modff(i * 0.03f, &tmp);
    EffectChain effects;
    return MakeAndAddRef<EffectSolidColor>(
        gfx::DeviceColor(red, 0.4f, 0.4f, 1.0f));
  }
};

class UploadBench : public BenchTest {
 public:
  UploadBench() : BenchTest("Upload Bench (10s 256x256 upload)") {}

  uint32_t* mBuf;
  RefPtr<DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    int bytesPerPixel = 4;
    int w = 256;
    int h = 256;
    mBuf = (uint32_t*)malloc(w * h * sizeof(uint32_t));

    mSurface = Factory::CreateWrappingDataSourceSurface(
        reinterpret_cast<uint8_t*>(mBuf), w * bytesPerPixel, IntSize(w, h),
        SurfaceFormat::B8G8R8A8);
    mTexture = aCompositor->CreateDataTextureSource();
  }

  virtual void Teardown(Compositor* aCompositor) {
    mSurface = nullptr;
    mTexture = nullptr;
    free(mBuf);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect,
                 size_t aStep) {
    for (size_t i = 0; i < aStep * 10; i++) {
      mTexture->Update(mSurface);
    }
  }
};

class TrivialTexturedQuadBench : public BenchTest {
 public:
  TrivialTexturedQuadBench()
      : BenchTest("Trvial Textured Quad (10s 256x256 quads)") {}

  uint32_t* mBuf;
  RefPtr<DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    int bytesPerPixel = 4;
    size_t w = 256;
    size_t h = 256;
    mBuf = (uint32_t*)malloc(w * h * sizeof(uint32_t));
    for (size_t i = 0; i < w * h; i++) {
      mBuf[i] = 0xFF00008F;
    }

    mSurface = Factory::CreateWrappingDataSourceSurface(
        reinterpret_cast<uint8_t*>(mBuf), w * bytesPerPixel, IntSize(w, h),
        SurfaceFormat::B8G8R8A8);
    mTexture = aCompositor->CreateDataTextureSource();
    mTexture->Update(mSurface);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect,
                 size_t aStep) {
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameTrivialQuad(aCompositor, aScreenRect, aStep, effects);
  }

  virtual void Teardown(Compositor* aCompositor) {
    mSurface = nullptr;
    mTexture = nullptr;
    free(mBuf);
  }

  already_AddRefed<Effect> CreateEffect(size_t i) {
    return CreateTexturedEffect(SurfaceFormat::B8G8R8A8, mTexture,
                                SamplingFilter::POINT, true);
  }
};

class StressTexturedQuadBench : public BenchTest {
 public:
  StressTexturedQuadBench()
      : BenchTest("Stress Textured Quad (10s 256x256 quads)") {}

  uint32_t* mBuf;
  RefPtr<DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    int bytesPerPixel = 4;
    size_t w = 256;
    size_t h = 256;
    mBuf = (uint32_t*)malloc(w * h * sizeof(uint32_t));
    for (size_t i = 0; i < w * h; i++) {
      mBuf[i] = 0xFF00008F;
    }

    mSurface = Factory::CreateWrappingDataSourceSurface(
        reinterpret_cast<uint8_t*>(mBuf), w * bytesPerPixel, IntSize(w, h),
        SurfaceFormat::B8G8R8A8);
    mTexture = aCompositor->CreateDataTextureSource();
    mTexture->Update(mSurface);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect,
                 size_t aStep) {
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameStressQuad(aCompositor, aScreenRect, aStep, effects);
  }

  virtual void Teardown(Compositor* aCompositor) {
    mSurface = nullptr;
    mTexture = nullptr;
    free(mBuf);
  }

  virtual already_AddRefed<Effect> CreateEffect(size_t i) {
    return CreateTexturedEffect(SurfaceFormat::B8G8R8A8, mTexture,
                                SamplingFilter::POINT, true);
  }
};

static void RunCompositorBench(Compositor* aCompositor,
                               const gfx::Rect& aScreenRect) {
  std::vector<BenchTest*> tests;

  tests.push_back(new EffectSolidColorBench());
  tests.push_back(new UploadBench());
  tests.push_back(new EffectSolidColorTrivialBench());
  tests.push_back(new EffectSolidColorStressBench());
  tests.push_back(new TrivialTexturedQuadBench());
  tests.push_back(new StressTexturedQuadBench());

  for (size_t i = 0; i < tests.size(); i++) {
    BenchTest* test = tests[i];
    std::vector<TimeDuration> results;
    int testsOverThreshold = 0;
    PROFILER_ADD_MARKER(test->ToString(), GRAPHICS);
    for (size_t j = 0; j < TEST_STEPS; j++) {
      test->Setup(aCompositor, j);

      TimeStamp start = TimeStamp::Now();
      IntRect screenRect(aScreenRect.x, aScreenRect.y, aScreenRect.width,
                         aScreenRect.height);
      aCompositor->BeginFrame(IntRect(screenRect.x, screenRect.y,
                                      screenRect.width, screenRect.height),
                              nullptr, aScreenRect, nullptr, nullptr);

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

void CompositorBench(Compositor* aCompositor, const gfx::IntRect& aScreenRect) {
  static bool sRanBenchmark = false;
  bool wantBenchmark = StaticPrefs::layers_bench_enabled();
  if (wantBenchmark && !sRanBenchmark) {
    RunCompositorBench(aCompositor, aScreenRect);
  }
  sRanBenchmark = wantBenchmark;
}

}  // namespace layers
}  // namespace mozilla

#endif
