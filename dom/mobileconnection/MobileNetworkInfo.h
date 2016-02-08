/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileNetworkInfo_h
#define mozilla_dom_MobileNetworkInfo_h

#include "mozilla/dom/MozMobileNetworkInfoBinding.h"
#include "nsIMobileNetworkInfo.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class MobileNetworkInfo final : public nsIMobileNetworkInfo
                              , public nsWrapperCache
{
public:
  NS_DECL_NSIMOBILENETWORKINFO
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MobileNetworkInfo)

  explicit MobileNetworkInfo(nsPIDOMWindowInner* aWindow);

  MobileNetworkInfo(const nsAString& aShortName, const nsAString& aLongName,
                    const nsAString& aMcc, const nsAString& aMnc,
                    const nsAString& aState);

  void
  Update(nsIMobileNetworkInfo* aInfo);

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  Nullable<MobileNetworkState>
  GetState() const
  {
    uint32_t i = 0;
    for (const EnumEntry* entry = MobileNetworkStateValues::strings;
         entry->value;
         ++entry, ++i) {
      if (mState.EqualsASCII(entry->value)) {
        return Nullable<MobileNetworkState>(static_cast<MobileNetworkState>(i));
      }
    }

    return Nullable<MobileNetworkState>();
  }

private:
  ~MobileNetworkInfo() {}

private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsString mShortName;
  nsString mLongName;
  nsString mMcc;
  nsString mMnc;
  nsString mState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MobileNetworkInfo_h
