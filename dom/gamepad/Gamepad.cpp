/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Gamepad.h"
#include "nsAutoPtr.h"
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Gamepad, mParent, mButtons)

Gamepad::Gamepad(nsISupports* aParent,
                 const nsAString& aID, uint32_t aIndex,
                 GamepadMappingType aMapping,
                 uint32_t aNumButtons, uint32_t aNumAxes)
  : mParent(aParent),
    mID(aID),
    mIndex(aIndex),
    mMapping(aMapping),
    mConnected(true),
    mButtons(aNumButtons),
    mAxes(aNumAxes)
{
  for (unsigned i = 0; i < aNumButtons; i++) {
    mButtons.InsertElementAt(i, new GamepadButton(mParent));
  }
  mAxes.InsertElementsAt(0, aNumAxes, 0.0f);
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
}

void
Gamepad::SetAxis(uint32_t aAxis, double aValue)
{
  MOZ_ASSERT(aAxis < mAxes.Length());
  if (mAxes[aAxis] != aValue) {
    mAxes[aAxis] = aValue;
    GamepadBinding::ClearCachedAxesValue(this);
  }
}

void
Gamepad::SyncState(Gamepad* aOther)
{
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
}

already_AddRefed<Gamepad>
Gamepad::Clone(nsISupports* aParent)
{
  nsRefPtr<Gamepad> out =
    new Gamepad(aParent, mID, mIndex, mMapping,
                mButtons.Length(), mAxes.Length());
  out->SyncState(this);
  return out.forget();
}

/* virtual */ JSObject*
Gamepad::WrapObject(JSContext* aCx)
{
  return GamepadBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
