/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxConfigManager_h
#define mozilla_gfx_config_gfxConfigManager_h

#include "gfxFeature.h"
#include "gfxTypes.h"
#include "nsCOMPtr.h"

class nsIGfxInfo;

namespace mozilla {
namespace gfx {

class gfxConfigManager {
 public:
  gfxConfigManager()
      : mFeatureWr(nullptr),
        mFeatureWrQualified(nullptr),
        mFeatureWrCompositor(nullptr),
        mFeatureWrAngle(nullptr),
        mFeatureWrDComp(nullptr),
        mFeatureWrPartial(nullptr),
        mFeatureWrShaderCache(nullptr),
        mFeatureWrOptimizedShaders(nullptr),
        mFeatureWrSoftware(nullptr),
        mFeatureHwCompositing(nullptr),
        mFeatureD3D11HwAngle(nullptr),
        mFeatureD3D11Compositing(nullptr),
        mFeatureGPUProcess(nullptr),
        mWrForceEnabled(false),
        mWrSoftwareForceEnabled(false),
        mWrCompositorForceEnabled(false),
        mWrForceAngle(false),
        mWrForceAngleNoGPUProcess(false),
        mWrDCompWinEnabled(false),
        mWrCompositorDCompRequired(false),
        mWrPartialPresent(false),
        mWrOptimizedShaders(false),
        mGPUProcessAllowSoftware(false),
        mWrEnvForceEnabled(false),
        mScaledResolution(false),
        mDisableHwCompositingNoWr(false),
        mIsNightly(false),
        mIsEarlyBetaOrEarlier(false),
        mSafeMode(false),
        mIsWin10OrLater(false) {}

  void Init();

  void ConfigureWebRender();
  void ConfigureFromBlocklist(long aFeature, FeatureState* aFeatureState);

 protected:
  void EmplaceUserPref(const char* aPrefName, Maybe<bool>& aValue);
  void ConfigureWebRenderQualified();

  nsCOMPtr<nsIGfxInfo> mGfxInfo;

  FeatureState* mFeatureWr;
  FeatureState* mFeatureWrQualified;
  FeatureState* mFeatureWrCompositor;
  FeatureState* mFeatureWrAngle;
  FeatureState* mFeatureWrDComp;
  FeatureState* mFeatureWrPartial;
  FeatureState* mFeatureWrShaderCache;
  FeatureState* mFeatureWrOptimizedShaders;
  FeatureState* mFeatureWrSoftware;

  FeatureState* mFeatureHwCompositing;
  FeatureState* mFeatureD3D11HwAngle;
  FeatureState* mFeatureD3D11Compositing;
  FeatureState* mFeatureGPUProcess;

  /**
   * Prefs
   */
  Maybe<bool> mWrCompositorEnabled;
  bool mWrForceEnabled;
  bool mWrSoftwareForceEnabled;
  bool mWrCompositorForceEnabled;
  bool mWrForceAngle;
  bool mWrForceAngleNoGPUProcess;
  bool mWrDCompWinEnabled;
  bool mWrCompositorDCompRequired;
  bool mWrPartialPresent;
  Maybe<bool> mWrShaderCache;
  bool mWrOptimizedShaders;
  bool mGPUProcessAllowSoftware;

  /**
   * Environment variables
   */
  bool mWrEnvForceEnabled;

  /**
   * System support
   */
  HwStretchingSupport mHwStretchingSupport;
  bool mScaledResolution;
  bool mDisableHwCompositingNoWr;
  bool mIsNightly;
  bool mIsEarlyBetaOrEarlier;
  bool mSafeMode;
  bool mIsWin10OrLater;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_config_gfxConfigParams_h
