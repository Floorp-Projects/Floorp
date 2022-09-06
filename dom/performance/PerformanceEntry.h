/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceEntry_h___
#define mozilla_dom_PerformanceEntry_h___

#include "nsDOMNavigationTiming.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "nsAtom.h"
#include "mozilla/dom/PerformanceObserverBinding.h"

class nsISupports;

namespace mozilla::dom {
class PerformanceResourceTiming;

// http://www.w3.org/TR/performance-timeline/#performanceentry
class PerformanceEntry : public nsISupports, public nsWrapperCache {
 protected:
  virtual ~PerformanceEntry();

 public:
  PerformanceEntry(nsISupports* aParent, const nsAString& aName,
                   const nsAString& aEntryType);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(PerformanceEntry)

  nsISupports* GetParentObject() const { return mParent; }

  void GetName(nsAString& aName) const {
    if (mName) {
      mName->ToString(aName);
    }
  }

  const nsAtom* GetName() const { return mName; }

  void GetEntryType(nsAString& aEntryType) const {
    if (mEntryType) {
      mEntryType->ToString(aEntryType);
    }
  }

  const nsAtom* GetEntryType() const { return mEntryType; }

  void SetEntryType(const nsAString& aEntryType) {
    mEntryType = NS_Atomize(aEntryType);
  }

  virtual DOMHighResTimeStamp StartTime() const { return 0; }

  virtual DOMHighResTimeStamp Duration() const { return 0; }

  virtual const PerformanceResourceTiming* ToResourceTiming() const {
    return nullptr;
  }

  virtual bool ShouldAddEntryToObserverBuffer(
      PerformanceObserverInit& aOption) const;

  virtual void BufferEntryIfNeeded() {}

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 protected:
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  nsCOMPtr<nsISupports> mParent;
  RefPtr<nsAtom> mName;
  RefPtr<nsAtom> mEntryType;
};

// Helper classes
class MOZ_STACK_CLASS PerformanceEntryComparator final {
 public:
  bool Equals(const PerformanceEntry* aElem1,
              const PerformanceEntry* aElem2) const {
    MOZ_ASSERT(aElem1 && aElem2, "Trying to compare null performance entries");
    return aElem1->StartTime() == aElem2->StartTime();
  }

  bool LessThan(const PerformanceEntry* aElem1,
                const PerformanceEntry* aElem2) const {
    MOZ_ASSERT(aElem1 && aElem2, "Trying to compare null performance entries");
    return aElem1->StartTime() < aElem2->StartTime();
  }
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_PerformanceEntry_h___ */
