/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadTouch_h
#define mozilla_dom_gamepad_GamepadTouch_h

#include "mozilla/dom/GamepadTouchBinding.h"
#include "mozilla/dom/GamepadTouchState.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class GamepadTouch final : public nsWrapperCache {
 public:
  explicit GamepadTouch(nsISupports* aParent);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(GamepadTouch)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(GamepadTouch)

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  uint32_t TouchId() const { return mTouchState.touchId; }
  uint32_t SurfaceId() const { return mTouchState.surfaceId; }
  void GetPosition(JSContext* aCx, JS::MutableHandle<JSObject*> aRetval,
                   ErrorResult& aRv);
  void GetSurfaceDimensions(JSContext* aCx,
                            JS::MutableHandle<JSObject*> aRetval,
                            ErrorResult& aRv);
  void SetTouchState(const GamepadTouchState& aTouch);
  void Set(const GamepadTouch* aOther);

 private:
  virtual ~GamepadTouch();

  nsCOMPtr<nsISupports> mParent;
  JS::Heap<JSObject*> mPosition;
  JS::Heap<JSObject*> mSurfaceDimensions;
  GamepadTouchState mTouchState;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_gamepad_GamepadTouch_h
