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
  NS_INTERFACE_MAP_ENTRY(nsIDOMGamepad)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(Gamepad, mParent)

Gamepad::Gamepad(nsISupports* aParent,
                 const nsAString& aID, uint32_t aIndex,
                 GamepadMappingType aMapping,
                 uint32_t aNumButtons, uint32_t aNumAxes)
  : mParent(aParent),
    mID(aID),
    mIndex(aIndex),
    mMapping(aMapping),
    mConnected(true)
{
  SetIsDOMBinding();
  mButtons.InsertElementsAt(0, aNumButtons);
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
  mButtons[aButton].pressed = aPressed;
  mButtons[aButton].value = aValue;
}

void
Gamepad::SetAxis(uint32_t aAxis, double aValue)
{
  MOZ_ASSERT(aAxis < mAxes.Length());
  mAxes[aAxis] = aValue;
}

nsresult
Gamepad::GetButtons(nsIVariant** aButtons)
{
  nsRefPtr<nsVariant> out = new nsVariant();
  NS_ENSURE_STATE(out);

  if (mButtons.Length() == 0) {
    nsresult rv = out->SetAsEmptyArray();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Note: The resulting nsIVariant dupes both the array and its elements.
    double* array = reinterpret_cast<double*>
                      (NS_Alloc(mButtons.Length() * sizeof(double)));
    NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

    for (uint32_t i = 0; i < mButtons.Length(); ++i) {
      array[i] = mButtons[i].value;
    }

    nsresult rv = out->SetAsArray(nsIDataType::VTYPE_DOUBLE,
                                  nullptr,
                                  mButtons.Length(),
                                  reinterpret_cast<void*>(array));
    NS_Free(array);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aButtons = out.forget().get();
  return NS_OK;
}

nsresult
Gamepad::GetAxes(nsIVariant** aAxes)
{
  nsRefPtr<nsVariant> out = new nsVariant();
  NS_ENSURE_STATE(out);

  if (mAxes.Length() == 0) {
    nsresult rv = out->SetAsEmptyArray();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Note: The resulting nsIVariant dupes both the array and its elements.
    double* array = reinterpret_cast<double*>
                              (NS_Alloc(mAxes.Length() * sizeof(double)));
    NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

    for (uint32_t i = 0; i < mAxes.Length(); ++i) {
      array[i] = mAxes[i];
    }

    nsresult rv = out->SetAsArray(nsIDataType::VTYPE_DOUBLE,
                                  nullptr,
                                  mAxes.Length(),
                                  reinterpret_cast<void*>(array));
    NS_Free(array);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aAxes = out.forget().get();
  return NS_OK;
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
    mButtons[i] = aOther->mButtons[i];
  }
  for (uint32_t i = 0; i < mAxes.Length(); ++i) {
    mAxes[i] = aOther->mAxes[i];
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
Gamepad::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return GamepadBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
