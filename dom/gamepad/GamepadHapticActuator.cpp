/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadHapticActuator.h"
#include "mozilla/dom/GamepadManager.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(GamepadHapticActuator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GamepadHapticActuator)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GamepadHapticActuator)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GamepadHapticActuator, mParent)

GamepadHapticActuator::GamepadHapticActuator(nsISupports* aParent, uint32_t aGamepadId,
                                             uint32_t aIndex)
  : mParent(aParent), mGamepadId(aGamepadId),
    mType(GamepadHapticActuatorType::Vibration), mIndex(aIndex)
{

}

/* virtual */ JSObject*
GamepadHapticActuator::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GamepadHapticActuator_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
GamepadHapticActuator::GetParentObject() const
{
  return mParent;
}

#define CLAMP(f, min, max) \
          (((f) < min)? min : (((f) > max) ? max : (f)))

already_AddRefed<Promise>
GamepadHapticActuator::Pulse(double aValue, double aDuration, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  MOZ_ASSERT(global);

  RefPtr<GamepadManager> gamepadManager(GamepadManager::GetService());
  MOZ_ASSERT(gamepadManager);

  // Clamp intensity aValue to be 0~1.
  double value = CLAMP(aValue, 0, 1);
  // aDuration should be always positive.
  double duration = CLAMP(aDuration, 0, aDuration);

  switch (mType) {
    case GamepadHapticActuatorType::Vibration:
    {
      RefPtr<Promise> promise =
        gamepadManager->VibrateHaptic(
          mGamepadId, mIndex, value, duration, global, aRv);
      if (!promise) {
        return nullptr;
      }
      return promise.forget();
    }
    default:
    {
      // We need to implement other types of haptic
      MOZ_ASSERT(false);
      return nullptr;
    }
  }
}

GamepadHapticActuatorType
GamepadHapticActuator::Type() const
{
  return mType;
}

void
GamepadHapticActuator::Set(const GamepadHapticActuator* aOther) {
  mGamepadId = aOther->mGamepadId;
  mType = aOther->mType;
  mIndex = aOther->mIndex;
}

} // namespace dom
} // namespace mozilla
