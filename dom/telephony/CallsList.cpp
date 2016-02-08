/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallsList.h"
#include "mozilla/dom/CallsListBinding.h"

#include "Telephony.h"
#include "TelephonyCall.h"
#include "TelephonyCallGroup.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CallsList,
                                      mTelephony,
                                      mGroup)

NS_IMPL_CYCLE_COLLECTING_ADDREF(CallsList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CallsList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CallsList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CallsList::CallsList(Telephony* aTelephony, TelephonyCallGroup* aGroup)
: mTelephony(aTelephony), mGroup(aGroup)
{
  MOZ_ASSERT(mTelephony);
}

CallsList::~CallsList()
{
}

nsPIDOMWindowInner*
CallsList::GetParentObject() const
{
  return mTelephony->GetOwner();
}

JSObject*
CallsList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CallsListBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<TelephonyCall>
CallsList::Item(uint32_t aIndex) const
{
  RefPtr<TelephonyCall> call;
  call = mGroup ? mGroup->CallsArray().SafeElementAt(aIndex) :
                  mTelephony->CallsArray().SafeElementAt(aIndex);

  return call.forget();
}

uint32_t
CallsList::Length() const
{
  return mGroup ? mGroup->CallsArray().Length() :
                  mTelephony->CallsArray().Length();
}

already_AddRefed<TelephonyCall>
CallsList::IndexedGetter(uint32_t aIndex, bool& aFound) const
{
  RefPtr<TelephonyCall> call;
  call = mGroup ? mGroup->CallsArray().SafeElementAt(aIndex) :
                  mTelephony->CallsArray().SafeElementAt(aIndex);
  aFound = call ? true : false;

  return call.forget();
}
