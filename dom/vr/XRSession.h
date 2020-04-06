/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRSession_h_
#define mozilla_dom_XRSession_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "nsRefreshDriver.h"

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
class VRDisplayClient;
}  // namespace gfx
namespace dom {

class XRSystem;
enum class XRReferenceSpaceType : uint8_t;
enum class XRSessionMode : uint8_t;
enum class XRVisibilityState : uint8_t;
class XRFrame;
class XRFrameRequestCallback;
class XRInputSource;
class XRInputSourceArray;
class XRLayer;
struct XRReferenceSpaceOptions;
class XRRenderState;
struct XRRenderStateInit;
class XRSpace;

class XRSession final : public DOMEventTargetHelper, public nsARefreshObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XRSession, DOMEventTargetHelper)

 private:
  explicit XRSession(
      nsPIDOMWindowInner* aWindow, XRSystem* aXRSystem,
      nsRefreshDriver* aRefreshDriver, gfx::VRDisplayClient* aClient,
      const nsTArray<XRReferenceSpaceType>& aEnabledReferenceSpaceTypes);

 public:
  static already_AddRefed<XRSession> CreateInlineSession(
      nsPIDOMWindowInner* aWindow, XRSystem* aXRSystem,
      const nsTArray<XRReferenceSpaceType>& aEnabledReferenceSpaceTypes);
  static already_AddRefed<XRSession> CreateImmersiveSession(
      nsPIDOMWindowInner* aWindow, XRSystem* aXRSystem,
      gfx::VRDisplayClient* aClient,
      const nsTArray<XRReferenceSpaceType>& aEnabledReferenceSpaceTypes);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Attributes
  XRVisibilityState VisibilityState();
  XRRenderState* RenderState();
  XRInputSourceArray* InputSources();

  // WebIDL Methods
  void UpdateRenderState(const XRRenderStateInit& aNewState, ErrorResult& aRv);
  already_AddRefed<Promise> RequestReferenceSpace(
      const XRReferenceSpaceType& aReferenceSpaceType, ErrorResult& aRv);
  int32_t RequestAnimationFrame(XRFrameRequestCallback& aCallback,
                                mozilla::ErrorResult& aError);
  void CancelAnimationFrame(int32_t aHandle, mozilla::ErrorResult& aError);
  already_AddRefed<Promise> End(ErrorResult& aRv);

  // WebIDL Events
  IMPL_EVENT_HANDLER(end);
  IMPL_EVENT_HANDLER(inputsourceschange);
  IMPL_EVENT_HANDLER(select);
  IMPL_EVENT_HANDLER(selectstart);
  IMPL_EVENT_HANDLER(selectend);
  IMPL_EVENT_HANDLER(squeeze);
  IMPL_EVENT_HANDLER(squeezestart);
  IMPL_EVENT_HANDLER(squeezeend);
  IMPL_EVENT_HANDLER(visibilitychange);

  // Non WebIDL Members
  gfx::VRDisplayClient* GetDisplayClient();
  XRRenderState* GetActiveRenderState();
  bool IsEnded() const;
  bool IsImmersive() const;
  MOZ_CAN_RUN_SCRIPT
  void StartFrame();

  // nsARefreshObserver
  MOZ_CAN_RUN_SCRIPT
  void WillRefresh(mozilla::TimeStamp aTime) override;

 protected:
  virtual ~XRSession();
  void LastRelease() override;
  void DisconnectFromOwner() override;
  void Shutdown();
  void ExitPresentInternal();
  void ApplyPendingRenderState();
  RefPtr<XRSystem> mXRSystem;
  bool mShutdown;
  bool mEnded;
  RefPtr<nsRefreshDriver> mRefreshDriver;
  RefPtr<gfx::VRDisplayClient> mDisplayClient;
  RefPtr<XRRenderState> mActiveRenderState;
  RefPtr<XRRenderState> mPendingRenderState;
  RefPtr<XRInputSourceArray> mInputSources;

  struct XRFrameRequest {
    XRFrameRequest(mozilla::dom::XRFrameRequestCallback& aCallback,
                   int32_t aHandle)
        : mCallback(&aCallback), mHandle(aHandle) {}
    MOZ_CAN_RUN_SCRIPT
    void Call(const DOMHighResTimeStamp& aTimeStamp, XRFrame& aFrame);

    // Comparator operators to allow RemoveElementSorted with an
    // integer argument on arrays of XRFrameRequest
    bool operator==(int32_t aHandle) const { return mHandle == aHandle; }
    bool operator<(int32_t aHandle) const { return mHandle < aHandle; }

    RefPtr<mozilla::dom::XRFrameRequestCallback> mCallback;
    int32_t mHandle;
  };

  int32_t mFrameRequestCallbackCounter;
  nsTArray<XRFrameRequest> mFrameRequestCallbacks;
  mozilla::TimeStamp mStartTimeStamp;
  nsTArray<XRReferenceSpaceType> mEnabledReferenceSpaceTypes;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRSession_h_
