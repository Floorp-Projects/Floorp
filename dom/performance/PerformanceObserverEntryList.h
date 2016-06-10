/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceObserverEntryList_h__
#define mozilla_dom_PerformanceObserverEntryList_h__

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/PerformanceEntryBinding.h"

namespace mozilla {
namespace dom {

struct PerformanceEntryFilterOptions;
class PerformanceEntry;
template<typename T> class Optional;

class PerformanceObserverEntryList final : public nsISupports,
                                           public nsWrapperCache
{
  ~PerformanceObserverEntryList();

public:
  PerformanceObserverEntryList(nsISupports* aOwner,
                               nsTArray<RefPtr<PerformanceEntry>>&
                               aEntries)
    : mOwner(aOwner)
    , mEntries(aEntries)
  {
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PerformanceObserverEntryList)

  void GetEntries(const PerformanceEntryFilterOptions& aFilter,
                  nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void GetEntriesByType(const nsAString& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void GetEntriesByName(const nsAString& aName,
                        const Optional<nsAString>& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);

private:
  nsCOMPtr<nsISupports> mOwner;
  nsTArray<RefPtr<PerformanceEntry>> mEntries;
};

} // namespace dom
} // namespace mozilla


#endif
