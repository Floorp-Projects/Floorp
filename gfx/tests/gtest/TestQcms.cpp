/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"
#include "gmock/gmock.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/SSE.h"
#include "mozilla/arm.h"
#include "qcms.h"
#include "qcmsint.h"

#include <cmath>

/* SSEv1 is only included in non-Windows or non-x86-64-bit builds. */
#if defined(MOZILLA_MAY_SUPPORT_SSE) && \
    (!(defined(_MSC_VER) && defined(_M_AMD64)))
#  define QCMS_MAY_SUPPORT_SSE
#endif

using namespace mozilla;

static bool CmpRgbChannel(const uint8_t* aRef, const uint8_t* aTest,
                          size_t aIndex) {
  return std::abs(static_cast<int8_t>(aRef[aIndex] - aTest[aIndex])) <= 1;
}

template <bool kSwapRB, bool kHasAlpha>
static bool CmpRgbBufferImpl(const uint8_t* aRefBuffer,
                             const uint8_t* aTestBuffer, size_t aPixels) {
  const size_t pixelSize = kHasAlpha ? 4 : 3;
  if (memcmp(aRefBuffer, aTestBuffer, aPixels * pixelSize) == 0) {
    return true;
  }

  const size_t kRIndex = kSwapRB ? 2 : 0;
  const size_t kGIndex = 1;
  const size_t kBIndex = kSwapRB ? 0 : 2;
  const size_t kAIndex = 3;

  size_t remaining = aPixels;
  const uint8_t* ref = aRefBuffer;
  const uint8_t* test = aTestBuffer;
  while (remaining > 0) {
    if (!CmpRgbChannel(ref, test, kRIndex) ||
        !CmpRgbChannel(ref, test, kGIndex) ||
        !CmpRgbChannel(ref, test, kBIndex) ||
        (kHasAlpha && ref[kAIndex] != test[kAIndex])) {
      EXPECT_EQ(test[kRIndex], ref[kRIndex]);
      EXPECT_EQ(test[kGIndex], ref[kGIndex]);
      EXPECT_EQ(test[kBIndex], ref[kBIndex]);
      if (kHasAlpha) {
        EXPECT_EQ(test[kAIndex], ref[kAIndex]);
      }
      return false;
    }

    --remaining;
    ref += pixelSize;
    test += pixelSize;
  }

  return true;
}

template <bool kSwapRB, bool kHasAlpha>
static size_t GetRgbInputBufferImpl(UniquePtr<uint8_t[]>& aOutBuffer) {
  const uint8_t colorSamples[] = {0, 5, 16, 43, 101, 127, 182, 255};
  const size_t colorSampleMax = sizeof(colorSamples) / sizeof(uint8_t);
  const size_t pixelSize = kHasAlpha ? 4 : 3;
  const size_t pixelCount = colorSampleMax * colorSampleMax * 256 * 3;

  aOutBuffer = MakeUnique<uint8_t[]>(pixelCount * pixelSize);
  if (!aOutBuffer) {
    return 0;
  }

  const size_t kRIndex = kSwapRB ? 2 : 0;
  const size_t kGIndex = 1;
  const size_t kBIndex = kSwapRB ? 0 : 2;
  const size_t kAIndex = 3;

  // Sample every red pixel value with a subset of green and blue.
  uint8_t* color = aOutBuffer.get();
  for (uint16_t r = 0; r < 256; ++r) {
    for (uint8_t g : colorSamples) {
      for (uint8_t b : colorSamples) {
        color[kRIndex] = r;
        color[kGIndex] = g;
        color[kBIndex] = b;
        if (kHasAlpha) {
          color[kAIndex] = 0x80;
        }
        color += pixelSize;
      }
    }
  }

  // Sample every green pixel value with a subset of red and blue.
  for (uint8_t r : colorSamples) {
    for (uint16_t g = 0; g < 256; ++g) {
      for (uint8_t b : colorSamples) {
        color[kRIndex] = r;
        color[kGIndex] = g;
        color[kBIndex] = b;
        if (kHasAlpha) {
          color[kAIndex] = 0x80;
        }
        color += pixelSize;
      }
    }
  }

  // Sample every blue pixel value with a subset of red and green.
  for (uint8_t r : colorSamples) {
    for (uint8_t g : colorSamples) {
      for (uint16_t b = 0; b < 256; ++b) {
        color[kRIndex] = r;
        color[kGIndex] = g;
        color[kBIndex] = b;
        if (kHasAlpha) {
          color[kAIndex] = 0x80;
        }
        color += pixelSize;
      }
    }
  }

  return pixelCount;
}

static size_t GetRgbInputBuffer(UniquePtr<uint8_t[]>& aOutBuffer) {
  return GetRgbInputBufferImpl<false, false>(aOutBuffer);
}

static size_t GetRgbaInputBuffer(UniquePtr<uint8_t[]>& aOutBuffer) {
  return GetRgbInputBufferImpl<false, true>(aOutBuffer);
}

static size_t GetBgraInputBuffer(UniquePtr<uint8_t[]>& aOutBuffer) {
  return GetRgbInputBufferImpl<true, true>(aOutBuffer);
}

static bool CmpRgbBuffer(const uint8_t* aRefBuffer, const uint8_t* aTestBuffer,
                         size_t aPixels) {
  return CmpRgbBufferImpl<false, false>(aRefBuffer, aTestBuffer, aPixels);
}

static bool CmpRgbaBuffer(const uint8_t* aRefBuffer, const uint8_t* aTestBuffer,
                          size_t aPixels) {
  return CmpRgbBufferImpl<false, true>(aRefBuffer, aTestBuffer, aPixels);
}

static bool CmpBgraBuffer(const uint8_t* aRefBuffer, const uint8_t* aTestBuffer,
                          size_t aPixels) {
  return CmpRgbBufferImpl<true, true>(aRefBuffer, aTestBuffer, aPixels);
}

static void ClearRgbBuffer(uint8_t* aBuffer, size_t aPixels) {
  if (aBuffer) {
    memset(aBuffer, 0, aPixels * 3);
  }
}

static void ClearRgbaBuffer(uint8_t* aBuffer, size_t aPixels) {
  if (aBuffer) {
    memset(aBuffer, 0, aPixels * 4);
  }
}

static UniquePtr<uint8_t[]> GetRgbOutputBuffer(size_t aPixels) {
  UniquePtr<uint8_t[]> buffer = MakeUnique<uint8_t[]>(aPixels * 3);
  ClearRgbBuffer(buffer.get(), aPixels);
  return buffer;
}

static UniquePtr<uint8_t[]> GetRgbaOutputBuffer(size_t aPixels) {
  UniquePtr<uint8_t[]> buffer = MakeUnique<uint8_t[]>(aPixels * 4);
  ClearRgbaBuffer(buffer.get(), aPixels);
  return buffer;
}

class GfxQcms_ProfilePairBase : public ::testing::Test {
 protected:
  GfxQcms_ProfilePairBase()
      : mInProfile(nullptr),
        mOutProfile(nullptr),
        mTransform(nullptr),
        mPixels(0),
        mStorageType(QCMS_DATA_RGB_8),
        mPrecache(false) {}

  void SetUp() override {
    // XXX: This means that we can't have qcms v2 unit test
    //      without changing the qcms API.
    qcms_enable_iccv4();
  }

  void TearDown() override {
    if (mInProfile) {
      qcms_profile_release(mInProfile);
    }
    if (mOutProfile) {
      qcms_profile_release(mOutProfile);
    }
    if (mTransform) {
      qcms_transform_release(mTransform);
    }
  }

  bool SetTransform(qcms_transform* aTransform) {
    if (mTransform) {
      qcms_transform_release(mTransform);
    }
    mTransform = aTransform;
    return !!mTransform;
  }

  bool SetTransform(qcms_data_type aType) {
    return SetTransform(qcms_transform_create(mInProfile, aType, mOutProfile,
                                              aType, QCMS_INTENT_DEFAULT));
  }

  bool SetBuffers(qcms_data_type aType) {
    switch (aType) {
      case QCMS_DATA_RGB_8:
        mPixels = GetRgbInputBuffer(mInput);
        mRef = GetRgbOutputBuffer(mPixels);
        mOutput = GetRgbOutputBuffer(mPixels);
        break;
      case QCMS_DATA_RGBA_8:
        mPixels = GetRgbaInputBuffer(mInput);
        mRef = GetRgbaOutputBuffer(mPixels);
        mOutput = GetRgbaOutputBuffer(mPixels);
        break;
      case QCMS_DATA_BGRA_8:
        mPixels = GetBgraInputBuffer(mInput);
        mRef = GetRgbaOutputBuffer(mPixels);
        mOutput = GetRgbaOutputBuffer(mPixels);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown type!");
        break;
    }

    mStorageType = aType;
    return mInput && mOutput && mRef && mPixels > 0u;
  }

  void ClearOutputBuffer() {
    switch (mStorageType) {
      case QCMS_DATA_RGB_8:
        ClearRgbBuffer(mOutput.get(), mPixels);
        break;
      case QCMS_DATA_RGBA_8:
      case QCMS_DATA_BGRA_8:
        ClearRgbaBuffer(mOutput.get(), mPixels);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown type!");
        break;
    }
  }

  void ProduceRef(transform_fn_t aFn) {
    aFn(mTransform, mInput.get(), mRef.get(), mPixels);
  }

  void CopyInputToRef() {
    size_t pixelSize = 0;
    switch (mStorageType) {
      case QCMS_DATA_RGB_8:
        pixelSize = 3;
        break;
      case QCMS_DATA_RGBA_8:
      case QCMS_DATA_BGRA_8:
        pixelSize = 4;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown type!");
        break;
    }

    memcpy(mRef.get(), mInput.get(), mPixels * pixelSize);
  }

  void ProduceOutput(transform_fn_t aFn) {
    ClearOutputBuffer();
    aFn(mTransform, mInput.get(), mOutput.get(), mPixels);
  }

  bool VerifyOutput(const UniquePtr<uint8_t[]>& aBuf) {
    switch (mStorageType) {
      case QCMS_DATA_RGB_8:
        return CmpRgbBuffer(aBuf.get(), mOutput.get(), mPixels);
      case QCMS_DATA_RGBA_8:
        return CmpRgbaBuffer(aBuf.get(), mOutput.get(), mPixels);
      case QCMS_DATA_BGRA_8:
        return CmpBgraBuffer(aBuf.get(), mOutput.get(), mPixels);
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown type!");
        break;
    }

    return false;
  }

  bool ProduceVerifyOutput(transform_fn_t aFn) {
    ProduceOutput(aFn);
    return VerifyOutput(mRef);
  }

  void PrecacheOutput() {
    qcms_profile_precache_output_transform(mOutProfile);
    mPrecache = true;
  }

  qcms_profile* mInProfile;
  qcms_profile* mOutProfile;
  qcms_transform* mTransform;

  UniquePtr<uint8_t[]> mInput;
  UniquePtr<uint8_t[]> mOutput;
  UniquePtr<uint8_t[]> mRef;
  size_t mPixels;
  qcms_data_type mStorageType;
  bool mPrecache;
};

class GfxQcms_sRGB_To_sRGB : public GfxQcms_ProfilePairBase {
 protected:
  void SetUp() override {
    GfxQcms_ProfilePairBase::SetUp();
    mInProfile = qcms_profile_sRGB();
    mOutProfile = qcms_profile_sRGB();
  }
};

class GfxQcms_sRGB_To_SamsungSyncmaster : public GfxQcms_ProfilePairBase {
 protected:
  void SetUp() override {
    GfxQcms_ProfilePairBase::SetUp();
    mInProfile = qcms_profile_sRGB();
    mOutProfile = qcms_profile_from_path("lcms_samsung_syncmaster.icc");
  }
};

class GfxQcms_sRGB_To_ThinkpadW540 : public GfxQcms_ProfilePairBase {
 protected:
  void SetUp() override {
    GfxQcms_ProfilePairBase::SetUp();
    mInProfile = qcms_profile_sRGB();
    mOutProfile = qcms_profile_from_path("lcms_thinkpad_w540.icc");
  }
};

TEST_F(GfxQcms_sRGB_To_sRGB, TransformIdentity) {
  PrecacheOutput();
  SetBuffers(QCMS_DATA_RGB_8);
  SetTransform(QCMS_DATA_RGB_8);
  qcms_transform_data(mTransform, mInput.get(), mOutput.get(), mPixels);
  EXPECT_TRUE(VerifyOutput(mInput));
}

class GfxQcmsPerf_Base : public GfxQcms_sRGB_To_ThinkpadW540 {
 protected:
  explicit GfxQcmsPerf_Base(qcms_data_type aType) { mStorageType = aType; }

  void TransformPerf() { ProduceRef(qcms_transform_data_rgb_out_lut_precache); }

  void TransformPlatformPerf() {
    qcms_transform_data(mTransform, mInput.get(), mRef.get(), mPixels);
  }

  void SetUp() override {
    GfxQcms_sRGB_To_ThinkpadW540::SetUp();
    PrecacheOutput();
    SetBuffers(mStorageType);
    SetTransform(mStorageType);
  }
};

class GfxQcmsPerf_Rgb : public GfxQcmsPerf_Base {
 protected:
  GfxQcmsPerf_Rgb() : GfxQcmsPerf_Base(QCMS_DATA_RGB_8) {}
};

class GfxQcmsPerf_Rgba : public GfxQcmsPerf_Base {
 protected:
  GfxQcmsPerf_Rgba() : GfxQcmsPerf_Base(QCMS_DATA_RGBA_8) {}
};

class GfxQcmsPerf_Bgra : public GfxQcmsPerf_Base {
 protected:
  GfxQcmsPerf_Bgra() : GfxQcmsPerf_Base(QCMS_DATA_BGRA_8) {}
};

MOZ_GTEST_BENCH_F(GfxQcmsPerf_Rgb, TransformC, [this] { TransformPerf(); });
MOZ_GTEST_BENCH_F(GfxQcmsPerf_Rgb, TransformPlatform,
                  [this] { TransformPlatformPerf(); });
MOZ_GTEST_BENCH_F(GfxQcmsPerf_Rgba, TransformC, [this] { TransformPerf(); });
MOZ_GTEST_BENCH_F(GfxQcmsPerf_Rgba, TransformPlatform,
                  [this] { TransformPlatformPerf(); });
MOZ_GTEST_BENCH_F(GfxQcmsPerf_Bgra, TransformC, [this] { TransformPerf(); });
MOZ_GTEST_BENCH_F(GfxQcmsPerf_Bgra, TransformPlatform,
                  [this] { TransformPlatformPerf(); });
