/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadPose_h
#define mozilla_dom_gamepad_GamepadPose_h

#include "mozilla/TypedEnumBits.h"
#include "mozilla/dom/Pose.h"
#include "mozilla/dom/GamepadPoseState.h"

namespace mozilla {
namespace dom {

class GamepadPose final : public Pose
{
public:
  GamepadPose(nsISupports* aParent, const GamepadPoseState& aState);
  explicit GamepadPose(nsISupports* aParent);

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool HasOrientation() const;
  bool HasPosition() const;
  virtual void GetPosition(JSContext* aJSContext,
                           JS::MutableHandle<JSObject*> aRetval,
                           ErrorResult& aRv) override;
  virtual void GetLinearVelocity(JSContext* aJSContext,
                                 JS::MutableHandle<JSObject*> aRetval,
                                 ErrorResult& aRv) override;
  virtual void GetLinearAcceleration(JSContext* aJSContext,
                                     JS::MutableHandle<JSObject*> aRetval,
                                     ErrorResult& aRv) override;
  virtual void GetOrientation(JSContext* aJSContext,
                              JS::MutableHandle<JSObject*> aRetval,
                              ErrorResult& aRv) override;
  virtual void GetAngularVelocity(JSContext* aJSContext,
                                  JS::MutableHandle<JSObject*> aRetval,
                                  ErrorResult& aRv) override;
  virtual void GetAngularAcceleration(JSContext* aJSContext,
                                      JS::MutableHandle<JSObject*> aRetval,
                                      ErrorResult& aRv) override;
  void SetPoseState(const GamepadPoseState& aPose);
  const GamepadPoseState& GetPoseState();

private:
  virtual ~GamepadPose();

  nsCOMPtr<nsISupports> mParent;
  GamepadPoseState mPoseState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_gamepad_GamepadPose_h
