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

#ifdef MOZ_WIDGET_GONK
#include "mozilla/layers/GrallocTextureHost.h"
#endif

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

static void
DrawFrameTrivialQuad(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep, const EffectChain& effects) {
  for (size_t i = 0; i < aStep * 10; i++) {
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

static void
DrawFrameStressQuad(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep, const EffectChain& effects)
{
  for (size_t i = 0; i < aStep * 10; i++) {
    const gfx::Rect& rect = gfx::Rect(aScreenRect.width * SimplePseudoRandom(i, 0),
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
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameTrivialQuad(aCompositor, aScreenRect, aStep, effects);
  }

  already_AddRefed<Effect> CreateEffect(size_t i) {
      float red;
      float tmp;
      red = modf(i * 0.03f, &tmp);
      EffectChain effects;
      gfxRGBA color(red, 0.4f, 0.4f, 1.0f);
      return MakeAndAddRef<EffectSolidColor>(gfx::Color(color.r,
                                                        color.g,
                                                        color.b,
                                                        color.a));
  }
};

class EffectSolidColorStressBench : public BenchTest {
public:
  EffectSolidColorStressBench()
    : BenchTest("EffectSolidColorStressBench (10s various EffectSolidColor)")
  {}

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameStressQuad(aCompositor, aScreenRect, aStep, effects);
  }

  already_AddRefed<Effect> CreateEffect(size_t i) {
      float red;
      float tmp;
      red = modf(i * 0.03f, &tmp);
      EffectChain effects;
      gfxRGBA color(red, 0.4f, 0.4f, 1.0f);
      return MakeAndAddRef<EffectSolidColor>(gfx::Color(color.r,
                                                        color.g,
                                                        color.b,
                                                        color.a));
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

class TrivialTexturedQuadBench : public BenchTest {
public:
  TrivialTexturedQuadBench()
    : BenchTest("Trvial Textured Quad (10s 256x256 quads)")
  {}

  uint32_t* mBuf;
  RefPtr<DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    int bytesPerPixel = 4;
    size_t w = 256;
    size_t h = 256;
    mBuf = (uint32_t *) malloc(w * h * sizeof(uint32_t));
    for (size_t i = 0; i < w * h; i++) {
      mBuf[i] = 0xFF00008F;
    }

    mSurface = Factory::CreateWrappingDataSourceSurface(
      reinterpret_cast<uint8_t*>(mBuf), w * bytesPerPixel, IntSize(w, h), SurfaceFormat::B8G8R8A8);
    mTexture = aCompositor->CreateDataTextureSource();
    mTexture->Update(mSurface);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
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
    return CreateTexturedEffect(SurfaceFormat::B8G8R8A8, mTexture, Filter::POINT, true);
  }
};

class StressTexturedQuadBench : public BenchTest {
public:
  StressTexturedQuadBench()
    : BenchTest("Stress Textured Quad (10s 256x256 quads)")
  {}

  uint32_t* mBuf;
  RefPtr<DataSourceSurface> mSurface;
  RefPtr<DataTextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    int bytesPerPixel = 4;
    size_t w = 256;
    size_t h = 256;
    mBuf = (uint32_t *) malloc(w * h * sizeof(uint32_t));
    for (size_t i = 0; i < w * h; i++) {
      mBuf[i] = 0xFF00008F;
    }

    mSurface = Factory::CreateWrappingDataSourceSurface(
      reinterpret_cast<uint8_t*>(mBuf), w * bytesPerPixel, IntSize(w, h), SurfaceFormat::B8G8R8A8);
    mTexture = aCompositor->CreateDataTextureSource();
    mTexture->Update(mSurface);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
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
    return CreateTexturedEffect(SurfaceFormat::B8G8R8A8, mTexture, Filter::POINT, true);
  }
};

#ifdef MOZ_WIDGET_GONK
class TrivialGrallocQuadBench : public BenchTest {
public:
  TrivialGrallocQuadBench()
    : BenchTest("Travial Gralloc Quad (10s 256x256 quads)")
  {}

  uint32_t* mBuf;
  android::sp<android::GraphicBuffer> mGralloc;
  RefPtr<TextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    mBuf = nullptr;
    int w = 256;
    int h = 256;
    int32_t format = android::PIXEL_FORMAT_RGBA_8888;;
    mGralloc = new android::GraphicBuffer(w, h,
                                 format,
                                 android::GraphicBuffer::USAGE_SW_READ_OFTEN |
                                 android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                                 android::GraphicBuffer::USAGE_HW_TEXTURE);
    mTexture = new mozilla::layers::GrallocTextureSourceOGL((CompositorOGL*)aCompositor, mGralloc.get(), SurfaceFormat::B8G8R8A8);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameTrivialQuad(aCompositor, aScreenRect, aStep, effects);
  }

  virtual void Teardown(Compositor* aCompositor) {
    mGralloc = nullptr;
    mTexture = nullptr;
    free(mBuf);
  }

  virtual already_AddRefed<Effect> CreateEffect(size_t i) {
    return CreateTexturedEffect(SurfaceFormat::B8G8R8A8, mTexture, Filter::POINT);
  }
};

class StressGrallocQuadBench : public BenchTest {
public:
  StressGrallocQuadBench()
    : BenchTest("Stress Gralloc Quad (10s 256x256 quads)")
  {}

  uint32_t* mBuf;
  android::sp<android::GraphicBuffer> mGralloc;
  RefPtr<TextureSource> mTexture;

  virtual void Setup(Compositor* aCompositor, size_t aStep) {
    mBuf = nullptr;
    int w = 256;
    int h = 256;
    int32_t format = android::PIXEL_FORMAT_RGBA_8888;;
    mGralloc = new android::GraphicBuffer(w, h,
                                 format,
                                 android::GraphicBuffer::USAGE_SW_READ_OFTEN |
                                 android::GraphicBuffer::USAGE_SW_WRITE_OFTEN |
                                 android::GraphicBuffer::USAGE_HW_TEXTURE);
    mTexture = new mozilla::layers::GrallocTextureSourceOGL((CompositorOGL*)aCompositor, mGralloc.get(), SurfaceFormat::B8G8R8A8);
  }

  void DrawFrame(Compositor* aCompositor, const gfx::Rect& aScreenRect, size_t aStep) {
    EffectChain effects;
    effects.mPrimaryEffect = CreateEffect(aStep);

    DrawFrameStressQuad(aCompositor, aScreenRect, aStep, effects);
  }

  virtual void Teardown(Compositor* aCompositor) {
    mGralloc = nullptr;
    mTexture = nullptr;
    free(mBuf);
  }

  virtual already_AddRefed<Effect> CreateEffect(size_t i) {
    return CreateTexturedEffect(SurfaceFormat::B8G8R8A8, mTexture, Filter::POINT);
  }
};
#endif

static void RunCompositorBench(Compositor* aCompositor, const gfx::Rect& aScreenRect)
{
  std::vector<BenchTest*> tests;

  tests.push_back(new EffectSolidColorBench());
  tests.push_back(new UploadBench());
  tests.push_back(new EffectSolidColorTrivialBench());
  tests.push_back(new EffectSolidColorStressBench());
  tests.push_back(new TrivialTexturedQuadBench());
  tests.push_back(new StressTexturedQuadBench());
#ifdef MOZ_WIDGET_GONK
  tests.push_back(new TrivialGrallocQuadBench());
  tests.push_back(new StressGrallocQuadBench());
#endif

  for (size_t i = 0; i < tests.size(); i++) {
    BenchTest* test = tests[i];
    std::vector<TimeDuration> results;
    int testsOverThreshold = 0;
    PROFILER_MARKER(test->ToString());
    for (size_t j = 0; j < TEST_STEPS; j++) {
      test->Setup(aCompositor, j);

      TimeStamp start = TimeStamp::Now();
      IntRect screenRect(aScreenRect.x, aScreenRect.y,
                           aScreenRect.width, aScreenRect.height);
      aCompositor->BeginFrame(
        IntRect(screenRect.x, screenRect.y,
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

void CompositorBench(Compositor* aCompositor, const gfx::Rect& aScreenRect)
{
  static bool sRanBenchmark = false;
  bool wantBenchmark = gfxPrefs::LayersBenchEnabled();
  if (wantBenchmark && !sRanBenchmark) {
    RunCompositorBench(aCompositor, aScreenRect);
  }
  sRanBenchmark = wantBenchmark;
}

} // namespace layers
} // namespace mozilla

#endif

