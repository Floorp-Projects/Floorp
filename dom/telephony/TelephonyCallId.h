/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TelephonyCallId_h
#define mozilla_dom_TelephonyCallId_h

#include "mozilla/dom/TelephonyCallIdBinding.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"

#include "nsWrapperCache.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class TelephonyCallId final : public nsISupports,
                              public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TelephonyCallId)

  TelephonyCallId(nsPIDOMWindow* aWindow, const nsAString& aNumber,
                  uint16_t aNumberPresentation, const nsAString& aName,
                  uint16_t aNamePresentation);

  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL

  void
  GetNumber(nsString& aNumber) const
  {
    aNumber.Assign(mNumber);
  }

  CallIdPresentation
  NumberPresentation() const;

  void
  GetName(nsString& aName) const
  {
    aName.Assign(mName);
  }

  CallIdPresentation
  NamePresentation() const;

  void
  UpdateNumber(const nsAString& aNumber)
  {
    mNumber = aNumber;
  }

private:
  ~TelephonyCallId();

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsString mNumber;
  uint16_t mNumberPresentation;
  nsString mName;
  uint16_t mNamePresentation;

  CallIdPresentation
  GetPresentationStr(uint16_t aPresentation) const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TelephonyCallId_h
