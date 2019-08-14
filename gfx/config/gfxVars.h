/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxVars_h
#define mozilla_gfx_config_gfxVars_h

#include <stdint.h>
#include "mozilla/Assertions.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/gfx/GraphicsMessages.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Types.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace gfx {

class gfxVarReceiver;

// Generator for graphics vars.
#define GFX_VARS_LIST(_)                                           \
  /* C++ Name,                  Data Type,        Default Value */ \
  _(BrowserTabsRemoteAutostart, bool, false)                       \
  _(ContentBackend, BackendType, BackendType::NONE)                \
  _(SoftwareBackend, BackendType, BackendType::NONE)               \
  _(TileSize, IntSize, IntSize(-1, -1))                            \
  _(UseXRender, bool, false)                                       \
  _(OffscreenFormat, gfxImageFormat,                               \
    mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32)                  \
  _(RequiresAcceleratedGLContextForCompositorOGL, bool, false)     \
  _(CanUseHardwareVideoDecoding, bool, false)                      \
  _(PDMWMFDisableD3D11Dlls, nsCString, nsCString())                \
  _(PDMWMFDisableD3D9Dlls, nsCString, nsCString())                 \
  _(DXInterop2Blocked, bool, false)                                \
  _(DXNV12Blocked, bool, false)                                    \
  _(DXP010Blocked, bool, false)                                    \
  _(DXP016Blocked, bool, false)                                    \
  _(UseWebRender, bool, false)                                     \
  _(UseWebRenderANGLE, bool, false)                                \
  _(UseWebRenderFlipSequentialWin, bool, false)                    \
  _(UseWebRenderDCompWin, bool, false)                             \
  _(UseWebRenderTripleBufferingWin, bool, false)                   \
  _(UseWebRenderProgramBinaryDisk, bool, false)                    \
  _(WorkaroundWebRenderIntelBug1556634, bool, false)               \
  _(WebRenderDebugFlags, int32_t, 0)                               \
  _(ScreenDepth, int32_t, 0)                                       \
  _(GREDirectory, nsString, nsString())                            \
  _(ProfDirectory, nsString, nsString())                           \
  _(UseOMTP, bool, false)                                          \
  _(AllowD3D11KeyedMutex, bool, false)                             \
  _(SystemTextQuality, int32_t, 5 /* CLEARTYPE_QUALITY */)         \
  _(LayersWindowRecordingPath, nsCString, nsCString())             \
  _(RemoteCanvasEnabled, bool, false)                              \
  _(UseDoubleBufferingWithCompositor, bool, false)                 \
  _(UseGLSwizzle, bool, true)

/* Add new entries above this line. */

// Some graphics settings are computed on the UI process and must be
// communicated to content and GPU processes. gfxVars helps facilitate
// this. Its function is similar to StaticPrefs, except rather than hold
// user preferences, it holds dynamically computed values.
//
// Each variable in GFX_VARS_LIST exposes the following static methods:
//
//    const DataType& CxxName();
//    void SetCxxName(const DataType& aValue);
//
// Note that the setter may only be called in the UI process; a gfxVar must be
// a variable that is determined in the UI process and pushed to child
// processes.
class gfxVars final {
 public:
  // These values will be used during the Initialize() call if set.  Any
  // updates that come before initialization will get added to this array.
  static void SetValuesForInitialize(
      const nsTArray<GfxVarUpdate>& aInitUpdates);

  static void Initialize();
  static void Shutdown();

  static void ApplyUpdate(const GfxVarUpdate& aUpdate);
  static void AddReceiver(gfxVarReceiver* aReceiver);
  static void RemoveReceiver(gfxVarReceiver* aReceiver);

  // Return a list of updates for all variables with non-default values.
  static nsTArray<GfxVarUpdate> FetchNonDefaultVars();

 public:
  // Each variable must expose Set and Get methods for IPDL.
  class VarBase {
   public:
    VarBase();
    virtual void SetValue(const GfxVarValue& aValue) = 0;
    virtual void GetValue(GfxVarValue* aOutValue) = 0;
    virtual bool HasDefaultValue() const = 0;
    size_t Index() const { return mIndex; }

   private:
    size_t mIndex;
  };

 private:
  static StaticAutoPtr<gfxVars> sInstance;
  static StaticAutoPtr<nsTArray<VarBase*>> sVarList;

  template <typename T, T Default()>
  class VarImpl final : public VarBase {
   public:
    VarImpl() : mValue(Default()) {}
    void SetValue(const GfxVarValue& aValue) override {
      aValue.get(&mValue);
      if (mListener) {
        mListener();
      }
    }
    void GetValue(GfxVarValue* aOutValue) override {
      *aOutValue = GfxVarValue(mValue);
    }
    bool HasDefaultValue() const override { return mValue == Default(); }
    const T& Get() const { return mValue; }
    // Return true if the value changed, false otherwise.
    bool Set(const T& aValue) {
      MOZ_ASSERT(XRE_IsParentProcess());
      if (mValue == aValue) {
        return false;
      }
      mValue = aValue;
      if (mListener) {
        mListener();
      }
      return true;
    }

    void SetListener(const std::function<void()>& aListener) {
      mListener = aListener;
    }

   private:
    T mValue;
    std::function<void()> mListener;
  };

#define GFX_VAR_DECL(CxxName, DataType, DefaultValue)                          \
 private:                                                                      \
  static DataType Get##CxxName##Default() { return DefaultValue; }             \
  VarImpl<DataType, Get##CxxName##Default> mVar##CxxName;                      \
                                                                               \
 public:                                                                       \
  static const DataType& CxxName() { return sInstance->mVar##CxxName.Get(); }  \
  static DataType Get##CxxName##OrDefault() {                                  \
    if (!sInstance) {                                                          \
      return DefaultValue;                                                     \
    }                                                                          \
    return sInstance->mVar##CxxName.Get();                                     \
  }                                                                            \
  static void Set##CxxName(const DataType& aValue) {                           \
    if (sInstance->mVar##CxxName.Set(aValue)) {                                \
      sInstance->NotifyReceivers(&sInstance->mVar##CxxName);                   \
    }                                                                          \
  }                                                                            \
                                                                               \
  static void Set##CxxName##Listener(const std::function<void()>& aListener) { \
    sInstance->mVar##CxxName.SetListener(aListener);                           \
  }

  GFX_VARS_LIST(GFX_VAR_DECL)
#undef GFX_VAR_DECL

 private:
  gfxVars();

  void NotifyReceivers(VarBase* aVar);

 private:
  nsTArray<gfxVarReceiver*> mReceivers;
};

#undef GFX_VARS_LIST

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_config_gfxVars_h
