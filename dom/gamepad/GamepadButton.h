/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadButton_h
#define mozilla_dom_gamepad_GamepadButton_h

#include <stdint.h>
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class GamepadButton : public nsISupports, public nsWrapperCache {
 public:
  explicit GamepadButton(nsISupports* aParent)
      : mParent(aParent), mValue(0), mPressed(false), mTouched(false) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GamepadButton)

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void SetPressed(bool aPressed) { mPressed = aPressed; }

  void SetTouched(bool aTouched) { mTouched = aTouched; }

  void SetValue(double aValue) { mValue = aValue; }

  bool Pressed() const { return mPressed; }

  bool Touched() const { return mTouched; }

  double Value() const { return mValue; }

 private:
  virtual ~GamepadButton() = default;

 protected:
  nsCOMPtr<nsISupports> mParent;
  double mValue;
  bool mPressed;
  bool mTouched;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_gamepad_GamepadButton_h
