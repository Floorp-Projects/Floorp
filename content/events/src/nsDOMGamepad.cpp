/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMGamepad.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsContentUtils.h"
#include "nsVariant.h"
#include "mozilla/dom/GamepadBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMGamepad)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMGamepad)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMGamepad)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGamepad)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsDOMGamepad, mParent)

nsDOMGamepad::nsDOMGamepad(nsISupports* aParent,
                           const nsAString& aID, uint32_t aIndex,
                           uint32_t aNumButtons, uint32_t aNumAxes)
  : mParent(aParent),
    mID(aID),
    mIndex(aIndex),
    mConnected(true)
{
  SetIsDOMBinding();
  mButtons.InsertElementsAt(0, aNumButtons, 0);
  mAxes.InsertElementsAt(0, aNumAxes, 0.0f);
}

void
nsDOMGamepad::SetIndex(uint32_t aIndex)
{
  mIndex = aIndex;
}

void
nsDOMGamepad::SetConnected(bool aConnected)
{
  mConnected = aConnected;
}

void
nsDOMGamepad::SetButton(uint32_t aButton, double aValue)
{
  MOZ_ASSERT(aButton < mButtons.Length());
  mButtons[aButton] = aValue;
}

void
nsDOMGamepad::SetAxis(uint32_t aAxis, double aValue)
{
  MOZ_ASSERT(aAxis < mAxes.Length());
  mAxes[aAxis] = aValue;
}

nsresult
nsDOMGamepad::GetButtons(nsIVariant** aButtons)
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
      array[i] = mButtons[i];
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
nsDOMGamepad::GetAxes(nsIVariant** aAxes)
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
nsDOMGamepad::SyncState(nsDOMGamepad* aOther)
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

already_AddRefed<nsDOMGamepad>
nsDOMGamepad::Clone(nsISupports* aParent)
{
  nsRefPtr<nsDOMGamepad> out =
    new nsDOMGamepad(aParent, mID, mIndex, mButtons.Length(), mAxes.Length());
  out->SyncState(this);
  return out.forget();
}

/* virtual */ JSObject*
nsDOMGamepad::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return GamepadBinding::Wrap(aCx, aScope, this);
}
