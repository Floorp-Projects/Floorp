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

class nsISupports;

namespace mozilla {
namespace dom {
class PerformanceResourceTiming;

// http://www.w3.org/TR/performance-timeline/#performanceentry
class PerformanceEntry : public nsISupports,
                         public nsWrapperCache
{
protected:
  virtual ~PerformanceEntry();

public:
  PerformanceEntry(nsISupports* aParent,
                   const nsAString& aName,
                   const nsAString& aEntryType);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PerformanceEntry)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  void GetName(nsAString& aName) const
  {
    aName = mName;
  }

  const nsAString& GetName() const
  {
    return mName;
  }

  void SetName(const nsAString& aName)
  {
    mName = aName;
  }

  void GetEntryType(nsAString& aEntryType) const
  {
    aEntryType = mEntryType;
  }

  const nsAString& GetEntryType()
  {
    return mEntryType;
  }

  void SetEntryType(const nsAString& aEntryType)
  {
    mEntryType = aEntryType;
  }

  virtual DOMHighResTimeStamp StartTime() const
  {
    return 0;
  }

  virtual DOMHighResTimeStamp Duration() const
  {
    return 0;
  }

  virtual const PerformanceResourceTiming* ToResourceTiming() const
  {
    return nullptr;
  }

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

protected:
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  nsCOMPtr<nsISupports> mParent;
  nsString mName;
  nsString mEntryType;
};

// Helper classes
class MOZ_STACK_CLASS PerformanceEntryComparator final
{
public:
  bool Equals(const PerformanceEntry* aElem1,
              const PerformanceEntry* aElem2) const
  {
    MOZ_ASSERT(aElem1 && aElem2,
               "Trying to compare null performance entries");
    return aElem1->StartTime() == aElem2->StartTime();
  }

  bool LessThan(const PerformanceEntry* aElem1,
                const PerformanceEntry* aElem2) const
  {
    MOZ_ASSERT(aElem1 && aElem2,
               "Trying to compare null performance entries");
    return aElem1->StartTime() < aElem2->StartTime();
  }
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PerformanceEntry_h___ */
