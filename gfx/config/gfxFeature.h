/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxFeature_h
#define mozilla_gfx_config_gfxFeature_h

#include <functional>
#include <stdint.h>
#include "gfxTelemetry.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "nsString.h"

namespace mozilla {
namespace gfx {

#define GFX_FEATURE_MAP(_)                                                   \
  /* Name,                        Type,         Description */               \
  _(HW_COMPOSITING, Feature, "Compositing")                                  \
  _(D3D11_COMPOSITING, Feature, "Direct3D11 Compositing")                    \
  _(OPENGL_COMPOSITING, Feature, "OpenGL Compositing")                       \
  _(DIRECT2D, Feature, "Direct2D")                                           \
  _(D3D11_HW_ANGLE, Feature, "Direct3D11 hardware ANGLE")                    \
  _(DIRECT_DRAW, Feature, "DirectDraw")                                      \
  _(GPU_PROCESS, Feature, "GPU Process")                                     \
  _(WEBRENDER, Feature, "WebRender")                                         \
  _(WEBRENDER_QUALIFIED, Feature, "WebRender qualified")                     \
  _(WEBRENDER_COMPOSITOR, Feature, "WebRender native compositor")            \
  _(WEBRENDER_PARTIAL, Feature, "WebRender partial present")                 \
  _(WEBRENDER_SHADER_CACHE, Feature, "WebRender shader disk cache")          \
  _(WEBRENDER_OPTIMIZED_SHADERS, Feature, "WebRender optimized shaders")     \
  _(WEBRENDER_ANGLE, Feature, "WebRender ANGLE")                             \
  _(WEBRENDER_DCOMP_PRESENT, Feature, "WebRender DirectComposition")         \
  _(WEBRENDER_SOFTWARE, Feature, "WebRender software fallback")              \
  _(OMTP, Feature, "Off Main Thread Painting")                               \
  _(WEBGPU, Feature, "WebGPU")                                               \
  _(X11_EGL, Feature, "X11 EGL")                                             \
  _(DMABUF, Feature, "DMABUF")                                               \
  _(WINDOW_OCCLUSION, Feature, "WINDOW_OCCLUSION")                           \
  _(VAAPI, Feature, "VA-API video decoding")                                 \
  _(VIDEO_OVERLAY, Feature, "video overlay")                                 \
  _(HW_DECODED_VIDEO_ZERO_COPY, Feature, "Hardware decoded video zero copy") \
  _(VP8_HW_DECODE, Feature, "VP8 hardware decoding")                         \
  _(VP9_HW_DECODE, Feature, "VP9 hardware decoding")
/* Add new entries above this comment */

enum class Feature : uint32_t {
#define MAKE_ENUM(name, type, desc) name,
  GFX_FEATURE_MAP(MAKE_ENUM)
#undef MAKE_ENUM
      NumValues
};

class FeatureState {
  friend class gfxConfig;
  friend class GfxConfigManager;  // for testing

 public:
  FeatureState() { Reset(); }

  bool IsEnabled() const;
  FeatureStatus GetValue() const;

  void EnableByDefault();
  void DisableByDefault(FeatureStatus aStatus, const char* aMessage,
                        const nsACString& aFailureId);
  bool SetDefault(bool aEnable, FeatureStatus aDisableStatus,
                  const char* aDisableMessage);
  bool InitOrUpdate(bool aEnable, FeatureStatus aDisableStatus,
                    const char* aMessage);
  void SetDefaultFromPref(const char* aPrefName, bool aIsEnablePref,
                          bool aDefaultValue, Maybe<bool> aUserValue);
  void SetDefaultFromPref(const char* aPrefName, bool aIsEnablePref,
                          bool aDefaultValue);
  void UserEnable(const char* aMessage);
  void UserForceEnable(const char* aMessage);
  void UserDisable(const char* aMessage, const nsACString& aFailureId);
  void Disable(FeatureStatus aStatus, const char* aMessage,
               const nsACString& aFailureId);
  void ForceDisable(FeatureStatus aStatus, const char* aMessage,
                    const nsACString& aFailureId) {
    SetFailed(aStatus, aMessage, aFailureId);
  }
  void SetFailed(FeatureStatus aStatus, const char* aMessage,
                 const nsACString& aFailureId);
  bool MaybeSetFailed(bool aEnable, FeatureStatus aStatus, const char* aMessage,
                      const nsACString& aFailureId);
  bool MaybeSetFailed(FeatureStatus aStatus, const char* aMessage,
                      const nsACString& aFailureId);

  // aType is "base", "user", "env", or "runtime".
  // aMessage may be null.
  typedef std::function<void(const char* aType, FeatureStatus aStatus,
                             const char* aMessage, const nsCString& aFailureId)>
      StatusIterCallback;
  void ForEachStatusChange(const StatusIterCallback& aCallback) const;

  const char* GetFailureMessage() const;
  const nsCString& GetFailureId() const;
  nsCString GetStatusAndFailureIdString() const;

  bool DisabledByDefault() const;

 private:
  void SetUser(FeatureStatus aStatus, const char* aMessage,
               const nsACString& aFailureId);
  void SetEnvironment(FeatureStatus aStatus, const char* aMessage,
                      const nsACString& aFailureId);
  void SetRuntime(FeatureStatus aStatus, const char* aMessage,
                  const nsACString& aFailureId);
  bool IsForcedOnByUser() const;
  const char* GetRuntimeMessage() const;
  bool IsInitialized() const { return mDefault.IsInitialized(); }

  void AssertInitialized() const { MOZ_ASSERT(IsInitialized()); }

  // Clear all state.
  void Reset();

 private:
  struct Instance {
    char mMessage[64];
    FeatureStatus mStatus;
    nsCString mFailureId;

    void Set(FeatureStatus aStatus);
    void Set(FeatureStatus aStatus, const char* aMessage,
             const nsACString& aFailureId);
    bool IsInitialized() const { return mStatus != FeatureStatus::Unused; }
    const char* MessageOrNull() const {
      return mMessage[0] != '\0' ? mMessage : nullptr;
    }
    const char* Message() const {
      MOZ_ASSERT(MessageOrNull());
      return mMessage;
    }
    const nsCString& FailureId() const { return mFailureId; }
  };

  // The default state is the state we decide on startup, based on the operating
  // system or a base preference.
  //
  // The user state factors in any changes to preferences that the user made.
  //
  // The environment state factors in any additional decisions made, such as
  // availability or blocklisting.
  //
  // The runtime state factors in any problems discovered at runtime.
  Instance mDefault;
  Instance mUser;
  Instance mEnvironment;
  Instance mRuntime;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_config_gfxFeature_h
