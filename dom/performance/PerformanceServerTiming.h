/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceServerTiming_h
#define mozilla_dom_PerformanceServerTiming_h

#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "nsString.h"

class nsIServerTiming;
class nsISupports;

namespace mozilla {
namespace dom {

class PerformanceServerTiming final : public nsISupports,
                                      public nsWrapperCache
{
public:
  PerformanceServerTiming(nsISupports* aParent, nsIServerTiming* aServerTiming)
    : mParent(aParent)
    , mServerTiming(aServerTiming)
  {
    MOZ_ASSERT(mServerTiming);
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PerformanceServerTiming)

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  void GetName(nsAString& aName) const;

  DOMHighResTimeStamp Duration() const;

  void GetDescription(nsAString& aDescription) const;

private:
  ~PerformanceServerTiming() = default;

  nsCOMPtr<nsISupports> mParent;
  nsCOMPtr<nsIServerTiming> mServerTiming;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PerformanceServerTiming_h
