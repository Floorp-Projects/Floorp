/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxConfigManager_h
#define mozilla_gfx_config_gfxConfigManager_h

#include "gfxFeature.h"

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
        mFeatureHwCompositing(nullptr),
        mFeatureD3D11HwAngle(nullptr),
        mFeatureGPUProcess(nullptr),
        mWrForceEnabled(false),
        mWrForceDisabled(false),
        mWrCompositorForceEnabled(false),
        mWrQualified(false),
        mWrForceAngle(false),
        mWrForceAngleNoGPUProcess(false),
        mWrDCompWinEnabled(false),
        mWrPictureCaching(false),
        mWrPartialPresent(false),
        mGPUProcessAllowSoftware(false),
        mWrEnvForceEnabled(false),
        mWrEnvForceDisabled(false),
        mHwStretchingSupport(false),
        mScaledResolution(false),
        mDisableHwCompositingNoWr(false),
        mIsNightly(false),
        mSafeMode(false),
        mIsWin10OrLater(false) {}

  void Init();

  void ConfigureWebRender();
  void ConfigureFromBlocklist(long aFeature, FeatureState* aFeatureState);

 protected:
  void EmplaceUserPref(const char* aPrefName, Maybe<bool>& aValue);
  bool ConfigureWebRenderQualified();

  nsCOMPtr<nsIGfxInfo> mGfxInfo;

  FeatureState* mFeatureWr;
  FeatureState* mFeatureWrQualified;
  FeatureState* mFeatureWrCompositor;
  FeatureState* mFeatureWrAngle;
  FeatureState* mFeatureWrDComp;
  FeatureState* mFeatureWrPartial;

  FeatureState* mFeatureHwCompositing;
  FeatureState* mFeatureD3D11HwAngle;
  FeatureState* mFeatureGPUProcess;

  /**
   * Prefs
   */
  Maybe<bool> mWrCompositorEnabled;
  bool mWrForceEnabled;
  bool mWrForceDisabled;
  bool mWrCompositorForceEnabled;
  bool mWrQualified;
  Maybe<bool> mWrQualifiedOverride;
  bool mWrForceAngle;
  bool mWrForceAngleNoGPUProcess;
  bool mWrDCompWinEnabled;
  bool mWrPictureCaching;
  bool mWrPartialPresent;
  bool mGPUProcessAllowSoftware;

  /**
   * Environment variables
   */
  bool mWrEnvForceEnabled;
  bool mWrEnvForceDisabled;

  /**
   * System support
   */
  bool mHwStretchingSupport;
  bool mScaledResolution;
  bool mDisableHwCompositingNoWr;
  bool mIsNightly;
  bool mSafeMode;
  bool mIsWin10OrLater;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_config_gfxConfigParams_h
