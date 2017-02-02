/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadHapticActuator_h
#define mozilla_dom_gamepad_GamepadHapticActuator_h

#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/GamepadHapticActuatorBinding.h"
#include "mozilla/dom/Gamepad.h"

namespace mozilla {
namespace dom {
class Promise;

class GamepadHapticActuator : public nsISupports,
                              public nsWrapperCache
{
public:
  GamepadHapticActuator(nsISupports* aParent, uint32_t aGamepadId,
                        uint32_t aIndex);
  explicit GamepadHapticActuator(nsISupports* aParent)
    : mParent(aParent), mType(GamepadHapticActuatorType::Vibration)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(GamepadHapticActuator)

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Pulse(double aValue, double aDuration, ErrorResult& aRv);

  GamepadHapticActuatorType Type() const;

  void Set(const GamepadHapticActuator* aOther);

private:
  virtual ~GamepadHapticActuator() {}

protected:
  nsCOMPtr<nsISupports> mParent;
  uint32_t mGamepadId;
  GamepadHapticActuatorType mType;
  uint32_t mIndex;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_gamepad_GamepadHapticActuator_h
