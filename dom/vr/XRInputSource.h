/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRInputSource_h_
#define mozilla_dom_XRInputSource_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
class VRDisplayClient;
}  // namespace gfx
namespace dom {
class Gamepad;
class XRSpace;
class XRSession;
class XRNativeOrigin;
enum class XRHandedness : uint8_t;
enum class XRTargetRayMode : uint8_t;

class XRInputSource final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(XRInputSource)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(XRInputSource)

  explicit XRInputSource(nsISupports* aParent);

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  XRHandedness Handedness();
  XRTargetRayMode TargetRayMode();
  XRSpace* TargetRaySpace();
  XRSpace* GetGripSpace();
  void GetProfiles(nsTArray<nsString>& aResult);
  Gamepad* GetGamepad();
  void Setup(XRSession* aSession, uint32_t aIndex);
  void SetGamepadIsConnected(bool aConnected);
  void Update(XRSession* aSession);
  int32_t GetIndex();

 protected:
  virtual ~XRInputSource() = default;

  nsCOMPtr<nsISupports> mParent;

 private:
  nsTArray<nsString> mProfiles;
  XRHandedness mHandedness;
  XRTargetRayMode mTargetRayMode;

  RefPtr<XRSpace> mTargetRaySpace;
  RefPtr<XRSpace> mGripSpace;
  int32_t mIndex;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRInputSource_h_
