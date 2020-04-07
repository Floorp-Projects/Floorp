/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRsystem_h_
#define mozilla_dom_XRsystem_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "nsContentPermissionHelper.h"
#include "VRManagerChild.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

struct XRSessionCreationOptions;

class IsSessionSupportedRequest {
 public:
  IsSessionSupportedRequest(XRSessionMode aSessionMode, Promise* aPromise)
      : mPromise(aPromise), mSessionMode(aSessionMode) {}
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(IsSessionSupportedRequest)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(IsSessionSupportedRequest)

  RefPtr<Promise> mPromise;
  XRSessionMode GetSessionMode() const;

 private:
  ~IsSessionSupportedRequest() = default;
  XRSessionMode mSessionMode;
};

class RequestSessionRequest {
 public:
  RequestSessionRequest(
      XRSessionMode aSessionMode, Promise* aPromise,
      const nsTArray<XRReferenceSpaceType>& aRequiredReferenceSpaceTypes,
      const nsTArray<XRReferenceSpaceType>& aOptionalReferenceSpaceTypes);
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(RequestSessionRequest)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(RequestSessionRequest)
  RefPtr<Promise> mPromise;

  bool ResolveSupport(
      const gfx::VRDisplayClient* aDisplay,
      nsTArray<XRReferenceSpaceType>& aEnabledReferenceSpaceTypes) const;
  bool IsImmersive() const;
  bool WantsHardware() const;
  bool NeedsHardware() const;
  XRSessionMode GetSessionMode() const;

 private:
  ~RequestSessionRequest() = default;
  XRSessionMode mSessionMode;
  nsTArray<XRReferenceSpaceType> mRequiredReferenceSpaceTypes;
  nsTArray<XRReferenceSpaceType> mOptionalReferenceSpaceTypes;
};

class XRRequestSessionPermissionRequest final
    : public ContentPermissionRequestBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XRRequestSessionPermissionRequest,
                                           ContentPermissionRequestBase)

  // nsIContentPermissionRequest
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::HandleValue choices) override;

  typedef std::function<void()> AllowCallback;
  typedef std::function<void()> AllowAnySiteCallback;
  typedef std::function<void()> CancelCallback;

  static already_AddRefed<XRRequestSessionPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow, AllowCallback&& aAllowCallback,
      AllowAnySiteCallback&& aAllowAnySiteCallback,
      CancelCallback&& aCancelCallback);

  typedef MozPromise<bool, bool, true> AutoGrantDelayPromise;
  RefPtr<AutoGrantDelayPromise> MaybeDelayAutomaticGrants();

 private:
  XRRequestSessionPermissionRequest(
      nsPIDOMWindowInner* aWindow, nsIPrincipal* aNodePrincipal,
      AllowCallback&& aAllowCallback,
      AllowAnySiteCallback&& aAllowAnySiteCallback,
      CancelCallback&& aCancelCallback);
  ~XRRequestSessionPermissionRequest();

  AllowCallback mAllowCallback;
  AllowAnySiteCallback mAllowAnySiteCallback;
  CancelCallback mCancelCallback;
  nsTArray<PermissionRequest> mPermissionRequests;
  bool mCallbackCalled;
};

class XRSystem final : public DOMEventTargetHelper,
                       public gfx::VRManagerEventObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XRSystem, DOMEventTargetHelper)

  void Shutdown();
  void SessionEnded(XRSession* aSession);
  bool FeaturePolicyBlocked() const;
  bool OnXRPermissionRequestAllow();
  void OnXRPermissionRequestCancel();
  bool HasActiveImmersiveSession() const;

  // WebIDL Boilerplate
  static already_AddRefed<XRSystem> Create(nsPIDOMWindowInner* aWindow);
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  already_AddRefed<Promise> IsSessionSupported(XRSessionMode aMode,
                                               ErrorResult& aRv);
  already_AddRefed<Promise> RequestSession(JSContext* aCx, XRSessionMode aMode,
                                           const XRSessionInit& aOptions,
                                           CallerType aCallerType,
                                           ErrorResult& aRv);
  IMPL_EVENT_HANDLER(devicechange);

  // VRManagerEventObserver interface
  void NotifyVRDisplayMounted(uint32_t aDisplayID) override;
  void NotifyVRDisplayUnmounted(uint32_t aDisplayID) override;
  void NotifyVRDisplayConnect(uint32_t aDisplayID) override;
  void NotifyVRDisplayDisconnect(uint32_t aDisplayID) override;
  void NotifyVRDisplayPresentChange(uint32_t aDisplayID) override;
  void NotifyPresentationGenerationChanged(uint32_t aDisplayID) override;
  void NotifyEnumerationCompleted() override;
  void NotifyDetectRuntimesCompleted() override;
  bool GetStopActivityStatus() const override;

 private:
  explicit XRSystem(nsPIDOMWindowInner* aWindow);
  virtual ~XRSystem() = default;
  void ResolveIsSessionSupportedRequests();
  void ProcessSessionRequestsWaitingForRuntimeDetection();
  bool CancelHardwareRequest(RequestSessionRequest* aRequest);
  void QueueSessionRequestWithEnumeration(RequestSessionRequest* aRequest);
  void QueueSessionRequestWithoutEnumeration(RequestSessionRequest* aRequest);
  void ResolveSessionRequestsWithoutHardware();
  void ResolveSessionRequests(
      nsTArray<RefPtr<RequestSessionRequest>>& aRequests,
      const nsTArray<RefPtr<gfx::VRDisplayClient>>& aDisplays);

  bool mShuttingDown;
  // https://immersive-web.github.io/webxr/#pending-immersive-session
  bool mPendingImmersiveSession;
  // https://immersive-web.github.io/webxr/#active-immersive-session
  RefPtr<XRSession> mActiveImmersiveSession;
  // https://immersive-web.github.io/webxr/#list-of-inline-sessions
  nsTArray<RefPtr<XRSession>> mInlineSessions;

  bool mEnumerationInFlight;

  nsTArray<RefPtr<IsSessionSupportedRequest>> mIsSessionSupportedRequests;
  nsTArray<RefPtr<RequestSessionRequest>>
      mRequestSessionRequestsWithoutHardware;
  nsTArray<RefPtr<RequestSessionRequest>>
      mRequestSessionRequestsWaitingForRuntimeDetection;
  nsTArray<RefPtr<RequestSessionRequest>>
      mRequestSessionRequestsWaitingForEnumeration;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRsystem_h_
