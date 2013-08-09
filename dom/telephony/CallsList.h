/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_CallsList_h__
#define mozilla_dom_telephony_CallsList_h__

#include "TelephonyCommon.h"

#include "nsWrapperCache.h"

BEGIN_TELEPHONY_NAMESPACE

class CallsList MOZ_FINAL : public nsISupports,
                            public nsWrapperCache
{
  nsRefPtr<Telephony> mTelephony;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CallsList)

  CallsList(Telephony* aTelephony);

  nsPIDOMWindow*
  GetParentObject() const;

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // CallsList WebIDL
  already_AddRefed<TelephonyCall>
  Item(uint32_t aIndex) const;

  uint32_t
  Length() const;

  already_AddRefed<TelephonyCall>
  IndexedGetter(uint32_t aIndex, bool& aFound) const;

private:
  ~CallsList();
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_CallsList_h__
