/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Gamepad.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"
#include "nsVariant.h"
#include "mozilla/dom/GamepadBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Gamepad)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Gamepad)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Gamepad)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Gamepad, mParent, mButtons, mPose,
                                      mHapticActuators)

void
Gamepad::UpdateTimestamp()
{
  nsCOMPtr<nsPIDOMWindowInner> newWindow(do_QueryInterface(mParent));
  if(newWindow) {
    Performance* perf = newWindow->GetPerformance();
    if (perf) {
      mTimestamp = perf->Now();
    }
  }
}

Gamepad::Gamepad(nsISupports* aParent,
                 const nsAString& aID, uint32_t aIndex,
                 uint32_t aHashKey,
                 GamepadMappingType aMapping,
                 GamepadHand aHand,
                 uint32_t aNumButtons, uint32_t aNumAxes,
                 uint32_t aNumHaptics)
  : mParent(aParent),
    mID(aID),
    mIndex(aIndex),
    mHashKey(aHashKey),
    mMapping(aMapping),
    mHand(aHand),
    mConnected(true),
    mButtons(aNumButtons),
    mAxes(aNumAxes),
    mTimestamp(0)
{
  for (unsigned i = 0; i < aNumButtons; i++) {
    mButtons.InsertElementAt(i, new GamepadButton(mParent));
  }
  mAxes.InsertElementsAt(0, aNumAxes, 0.0f);
  mPose = new GamepadPose(aParent);
  for (uint32_t i = 0; i < aNumHaptics; ++i) {
    mHapticActuators.AppendElement(new GamepadHapticActuator(mParent, mHashKey, i));
  }
  UpdateTimestamp();
}

void
Gamepad::SetIndex(uint32_t aIndex)
{
  mIndex = aIndex;
}

void
Gamepad::SetConnected(bool aConnected)
{
  mConnected = aConnected;
}

void
Gamepad::SetButton(uint32_t aButton, bool aPressed, double aValue)
{
  MOZ_ASSERT(aButton < mButtons.Length());
  mButtons[aButton]->SetPressed(aPressed);
  mButtons[aButton]->SetValue(aValue);
  UpdateTimestamp();
}

void
Gamepad::SetAxis(uint32_t aAxis, double aValue)
{
  MOZ_ASSERT(aAxis < mAxes.Length());
  if (mAxes[aAxis] != aValue) {
    mAxes[aAxis] = aValue;
    GamepadBinding::ClearCachedAxesValue(this);
  }
  UpdateTimestamp();
}

void
Gamepad::SetPose(const GamepadPoseState& aPose)
{
  mPose->SetPoseState(aPose);
}

void
Gamepad::SyncState(Gamepad* aOther)
{
  const char* kGamepadExtEnabledPref = "dom.gamepad.extensions.enabled";

  if (mButtons.Length() != aOther->mButtons.Length() ||
      mAxes.Length() != aOther->mAxes.Length()) {
    return;
  }

  mConnected = aOther->mConnected;
  for (uint32_t i = 0; i < mButtons.Length(); ++i) {
    mButtons[i]->SetPressed(aOther->mButtons[i]->Pressed());
    mButtons[i]->SetValue(aOther->mButtons[i]->Value());
  }

  bool changed = false;
  for (uint32_t i = 0; i < mAxes.Length(); ++i) {
    changed = changed || (mAxes[i] != aOther->mAxes[i]);
    mAxes[i] = aOther->mAxes[i];
  }
  if (changed) {
    GamepadBinding::ClearCachedAxesValue(this);
  }

  if (Preferences::GetBool(kGamepadExtEnabledPref)) {
    MOZ_ASSERT(aOther->GetPose());
    mPose->SetPoseState(aOther->GetPose()->GetPoseState());
    mHand = aOther->Hand();
    for (uint32_t i = 0; i < mHapticActuators.Length(); ++i) {
      mHapticActuators[i]->Set(aOther->mHapticActuators[i]);
    }
  }

  UpdateTimestamp();
}

already_AddRefed<Gamepad>
Gamepad::Clone(nsISupports* aParent)
{
  RefPtr<Gamepad> out =
    new Gamepad(aParent, mID, mIndex, mHashKey, mMapping,
                mHand, mButtons.Length(), mAxes.Length(),
                mHapticActuators.Length());
  out->SyncState(this);
  return out.forget();
}

/* virtual */ JSObject*
Gamepad::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GamepadBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
