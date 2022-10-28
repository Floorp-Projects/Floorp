/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"

#include "gfxFeature.h"
#include "mozilla/gfx/gfxConfigManager.h"
#include "nsIGfxInfo.h"

using namespace mozilla;
using namespace mozilla::gfx;

namespace mozilla {
namespace gfx {

class MockGfxInfo final : public nsIGfxInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  int32_t mStatusWr;
  int32_t mStatusWrCompositor;
  int32_t mStatusWrShaderCache;
  int32_t mStatusWrOptimizedShaders;
  int32_t mStatusWrPartialPresent;
  int32_t mMaxRefreshRate;
  bool mHasMixedRefreshRate;
  Maybe<bool> mHasBattery;
  const char* mVendorId;
  const char* mDeviceId;

  // Default allows WebRender + compositor, and is desktop NVIDIA.
  MockGfxInfo()
      : mStatusWr(nsIGfxInfo::FEATURE_ALLOW_ALWAYS),
        mStatusWrCompositor(nsIGfxInfo::FEATURE_STATUS_OK),
        mStatusWrShaderCache(nsIGfxInfo::FEATURE_STATUS_OK),
        mStatusWrOptimizedShaders(nsIGfxInfo::FEATURE_STATUS_OK),
        mStatusWrPartialPresent(nsIGfxInfo::FEATURE_STATUS_OK),
        mMaxRefreshRate(-1),
        mHasMixedRefreshRate(false),
        mHasBattery(Some(false)),
        mVendorId("0x10de"),
        mDeviceId("") {}

  NS_IMETHOD GetFeatureStatus(int32_t aFeature, nsACString& aFailureId,
                              int32_t* _retval) override {
    switch (aFeature) {
      case nsIGfxInfo::FEATURE_WEBRENDER:
        *_retval = mStatusWr;
        break;
      case nsIGfxInfo::FEATURE_WEBRENDER_COMPOSITOR:
        *_retval = mStatusWrCompositor;
        break;
      case nsIGfxInfo::FEATURE_WEBRENDER_SHADER_CACHE:
        *_retval = mStatusWrShaderCache;
        break;
      case nsIGfxInfo::FEATURE_WEBRENDER_OPTIMIZED_SHADERS:
        *_retval = mStatusWrOptimizedShaders;
        break;
      case nsIGfxInfo::FEATURE_WEBRENDER_PARTIAL_PRESENT:
        *_retval = mStatusWrPartialPresent;
        break;
      default:
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    return NS_OK;
  }

  NS_IMETHOD GetHasBattery(bool* aHasBattery) override {
    if (mHasBattery.isNothing()) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    *aHasBattery = *mHasBattery;
    return NS_OK;
  }

  NS_IMETHOD GetAdapterVendorID(nsAString& aAdapterVendorID) override {
    if (!mVendorId) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    aAdapterVendorID.AssignASCII(mVendorId);
    return NS_OK;
  }

  NS_IMETHOD GetAdapterDeviceID(nsAString& aAdapterDeviceID) override {
    if (!mDeviceId) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    aAdapterDeviceID.AssignASCII(mDeviceId);
    return NS_OK;
  }

  NS_IMETHOD_(int32_t) GetMaxRefreshRate(bool* aMixed) override {
    if (aMixed) {
      *aMixed = mHasMixedRefreshRate;
    }
    return mMaxRefreshRate;
  }

  NS_IMETHODIMP GetEmbeddedInFirefoxReality(
      bool* aEmbeddedInFirefoxReality) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetAzureCanvasBackend(nsAString& aBackend) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetAzureContentBackend(nsAString& aBackend) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // The following methods we don't need for testing gfxConfigManager.
  NS_IMETHOD GetFeatureSuggestedDriverVersion(int32_t aFeature,
                                              nsAString& _retval) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetMonitors(JSContext* cx,
                         JS::MutableHandle<JS::Value> _retval) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetFailures(nsTArray<int32_t>& indices,
                         nsTArray<nsCString>& failures) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD_(void) LogFailure(const nsACString& failure) override {}
  NS_IMETHOD GetInfo(JSContext*, JS::MutableHandle<JS::Value>) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetFeatures(JSContext*, JS::MutableHandle<JS::Value>) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetFeatureLog(JSContext*, JS::MutableHandle<JS::Value>) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetActiveCrashGuards(JSContext*,
                                  JS::MutableHandle<JS::Value>) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetContentBackend(nsAString& aContentBackend) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetUsingGPUProcess(bool* aOutValue) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetWebRenderEnabled(bool* aWebRenderEnabled) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetIsHeadless(bool* aIsHeadless) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetTargetFrameRate(uint32_t* aTargetFrameRate) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetCodecSupportInfo(nsACString& aCodecSupportInfo) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetD2DEnabled(bool* aD2DEnabled) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetDWriteEnabled(bool* aDWriteEnabled) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetDWriteVersion(nsAString& aDwriteVersion) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetCleartypeParameters(nsAString& aCleartypeParams) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetWindowProtocol(nsAString& aWindowProtocol) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetDesktopEnvironment(nsAString& aDesktopEnvironment) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetTestType(nsAString& aTestType) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDescription(nsAString& aAdapterDescription) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriver(nsAString& aAdapterDriver) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterSubsysID(nsAString& aAdapterSubsysID) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterRAM(uint32_t* aAdapterRAM) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriverVendor(nsAString& aAdapterDriverVendor) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriverVersion(
      nsAString& aAdapterDriverVersion) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriverDate(nsAString& aAdapterDriverDate) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDescription2(nsAString& aAdapterDescription) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriver2(nsAString& aAdapterDriver) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterVendorID2(nsAString& aAdapterVendorID) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDeviceID2(nsAString& aAdapterDeviceID) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterSubsysID2(nsAString& aAdapterSubsysID) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterRAM2(uint32_t* aAdapterRAM) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriverVendor2(nsAString& aAdapterDriverVendor) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriverVersion2(
      nsAString& aAdapterDriverVersion) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetAdapterDriverDate2(nsAString& aAdapterDriverDate) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetIsGPU2Active(bool* aIsGPU2Active) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetDrmRenderDevice(nsACString& aDrmRenderDevice) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD ControlGPUProcessForXPCShell(bool aEnable,
                                          bool* _retval) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD KillGPUProcessForTests() override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD CrashGPUProcessForTests() override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD_(void) GetData() override {}

 private:
  virtual ~MockGfxInfo() = default;
};

NS_IMPL_ISUPPORTS(MockGfxInfo, nsIGfxInfo)

class GfxConfigManager : public ::testing::Test, public gfxConfigManager {
 public:
  GfxConfigManager() {
    mFeatureWr = &mFeatures.mWr;
    mFeatureWrQualified = &mFeatures.mWrQualified;
    mFeatureWrCompositor = &mFeatures.mWrCompositor;
    mFeatureWrAngle = &mFeatures.mWrAngle;
    mFeatureWrDComp = &mFeatures.mWrDComp;
    mFeatureWrPartial = &mFeatures.mWrPartial;
    mFeatureWrShaderCache = &mFeatures.mWrShaderCache;
    mFeatureWrOptimizedShaders = &mFeatures.mWrOptimizedShaders;
    mFeatureHwCompositing = &mFeatures.mHwCompositing;
    mFeatureGPUProcess = &mFeatures.mGPUProcess;
    mFeatureWrSoftware = &mFeatures.mWrSoftware;
  }

  void SetUp() override {
    mMockGfxInfo = new MockGfxInfo();
    mGfxInfo = mMockGfxInfo;

    mFeatureD3D11HwAngle = &mFeatures.mD3D11HwAngle;
    mFeatureD3D11Compositing = &mFeatures.mD3D11Compositing;

    // By default, turn everything on. This effectively assumes we are on
    // qualified nightly Windows 10 with DirectComposition support.
    mFeatureHwCompositing->EnableByDefault();
    mFeatureD3D11HwAngle->EnableByDefault();
    mFeatureD3D11Compositing->EnableByDefault();
    mFeatureGPUProcess->EnableByDefault();

    mWrCompositorEnabled.emplace(true);
    mWrPartialPresent = true;
    mWrShaderCache.emplace(true);
    mWrOptimizedShaders = true;
    mWrForceAngle = true;
    mWrDCompWinEnabled = true;
    mWrCompositorDCompRequired = true;
    ++mHwStretchingSupport.mBoth;
    mIsWin10OrLater = true;
    mIsNightly = true;
    mIsEarlyBetaOrEarlier = true;
  }

  void TearDown() override {
    mFeatures.mWr.Reset();
    mFeatures.mWrQualified.Reset();
    mFeatures.mWrCompositor.Reset();
    mFeatures.mWrAngle.Reset();
    mFeatures.mWrDComp.Reset();
    mFeatures.mWrPartial.Reset();
    mFeatures.mWrShaderCache.Reset();
    mFeatures.mWrOptimizedShaders.Reset();
    mFeatures.mHwCompositing.Reset();
    mFeatures.mD3D11HwAngle.Reset();
    mFeatures.mD3D11Compositing.Reset();
    mFeatures.mGPUProcess.Reset();
    mFeatures.mWrSoftware.Reset();
  }

  struct {
    FeatureState mWr;
    FeatureState mWrQualified;
    FeatureState mWrCompositor;
    FeatureState mWrAngle;
    FeatureState mWrDComp;
    FeatureState mWrPartial;
    FeatureState mWrShaderCache;
    FeatureState mWrOptimizedShaders;
    FeatureState mHwCompositing;
    FeatureState mD3D11HwAngle;
    FeatureState mD3D11Compositing;
    FeatureState mGPUProcess;
    FeatureState mWrSoftware;
  } mFeatures;

  RefPtr<MockGfxInfo> mMockGfxInfo;
};

}  // namespace gfx
}  // namespace mozilla

TEST_F(GfxConfigManager, WebRenderDefault) {
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderNoPartialPresent) {
  mWrPartialPresent = false;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderPartialBlocked) {
  mWrPartialPresent = true;
  mMockGfxInfo->mStatusWrPartialPresent = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrPartial.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderForcePartialBlocked) {
  mWrForcePartialPresent = true;
  mMockGfxInfo->mStatusWrPartialPresent = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderScaledResolutionWithHwStretching) {
  mScaledResolution = true;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderScaledResolutionNoHwStretching) {
  ++mHwStretchingSupport.mNone;
  mScaledResolution = true;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderEnabledWithDisableHwCompositingNoWr) {
  mDisableHwCompositingNoWr = true;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderDisabledWithDisableHwCompositingNoWr) {
  mDisableHwCompositingNoWr = true;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_FALSE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_FALSE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderDisabledWithAllowSoftwareGPUProcess) {
  mDisableHwCompositingNoWr = true;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  mGPUProcessAllowSoftware = true;
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_FALSE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderSafeMode) {
  mSafeMode = true;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderEarlierThanWindows10) {
  mIsWin10OrLater = false;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderDCompDisabled) {
  mWrDCompWinEnabled = false;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderDCompNotRequired) {
  mWrDCompWinEnabled = false;
  mWrCompositorDCompRequired = false;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderForceAngleDisabled) {
  mWrForceAngle = false;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderD3D11HwAngleDisabled) {
  mFeatures.mD3D11HwAngle.UserDisable("", ""_ns);
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_FALSE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderD3D11HwAngleAndForceAngleDisabled) {
  mWrForceAngle = false;
  mFeatures.mD3D11HwAngle.UserDisable("", ""_ns);
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_FALSE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderGPUProcessDisabled) {
  mFeatures.mGPUProcess.UserDisable("", ""_ns);
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_FALSE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderQualifiedAndBlocklistAllowQualified) {
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_ALLOW_QUALIFIED;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderQualifiedAndBlocklistAllowAlways) {
  mIsNightly = mIsEarlyBetaOrEarlier = false;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_ALLOW_ALWAYS;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderIntelBatteryNoHwStretchingNotNightly) {
  mIsNightly = mIsEarlyBetaOrEarlier = false;
  ++mHwStretchingSupport.mNone;
  mScaledResolution = true;
  mMockGfxInfo->mHasBattery.ref() = true;
  mMockGfxInfo->mVendorId = "0x8086";
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderNvidiaHighMixedRefreshRateNightly) {
  mIsNightly = true;
  mMockGfxInfo->mMaxRefreshRate = 120;
  mMockGfxInfo->mHasMixedRefreshRate = true;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderNvidiaHighMixedRefreshRateNotNightly) {
  mIsNightly = mIsEarlyBetaOrEarlier = false;
  mMockGfxInfo->mMaxRefreshRate = 120;
  mMockGfxInfo->mHasMixedRefreshRate = true;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderNvidiaHighRefreshRateNotNightly) {
  mMockGfxInfo->mMaxRefreshRate = 120;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderNvidiaLowMixedRefreshRateNotNightly) {
  mMockGfxInfo->mMaxRefreshRate = 60;
  mMockGfxInfo->mHasMixedRefreshRate = true;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderSofwareAndNotQualified) {
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderForceEnabledEnvvar) {
  mWrEnvForceEnabled = true;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderSoftwareAllowedForceEnabledPref) {
  mWrForceEnabled = true;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderForceSoftwareForceEnabledEnvvar) {
  mWrEnvForceEnabled = true;
  mWrSoftwareForceEnabled = true;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderSoftwareReleaseWindowsGPUProcessDisabled) {
  mIsNightly = mIsEarlyBetaOrEarlier = false;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  mFeatures.mGPUProcess.UserDisable("", ""_ns);
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_FALSE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderSoftwareReleaseGPUProcessDisabled) {
  mIsNightly = mIsEarlyBetaOrEarlier = false;
  mIsWin10OrLater = false;
  mFeatureD3D11Compositing = nullptr;
  mMockGfxInfo->mStatusWr = nsIGfxInfo::FEATURE_DENIED;
  mFeatures.mGPUProcess.UserDisable("", ""_ns);
  ConfigureWebRender();

  EXPECT_FALSE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_FALSE(mFeatures.mWr.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_FALSE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderShaderCacheDisabled) {
  mWrShaderCache = Some(false);
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}

TEST_F(GfxConfigManager, WebRenderShaderCacheBlocked) {
  mMockGfxInfo->mStatusWrShaderCache = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  ConfigureWebRender();

  EXPECT_TRUE(mFeatures.mWrQualified.IsEnabled());
  EXPECT_TRUE(mFeatures.mWr.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrCompositor.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrDComp.IsEnabled());
  EXPECT_FALSE(mFeatures.mWrShaderCache.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrOptimizedShaders.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrPartial.IsEnabled());
  EXPECT_TRUE(mFeatures.mHwCompositing.IsEnabled());
  EXPECT_TRUE(mFeatures.mGPUProcess.IsEnabled());
  EXPECT_TRUE(mFeatures.mD3D11HwAngle.IsEnabled());
  EXPECT_TRUE(mFeatures.mWrSoftware.IsEnabled());
}
