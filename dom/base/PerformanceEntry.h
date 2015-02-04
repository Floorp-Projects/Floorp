/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceEntry_h___
#define mozilla_dom_PerformanceEntry_h___

#include "nsPerformance.h"
#include "nsDOMNavigationTiming.h"

namespace mozilla {
namespace dom {

// http://www.w3.org/TR/performance-timeline/#performanceentry
class PerformanceEntry : public nsISupports,
                         public nsWrapperCache
{
protected:
  virtual ~PerformanceEntry();

public:
  PerformanceEntry(nsPerformance* aPerformance,
                   const nsAString& aName,
                   const nsAString& aEntryType);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PerformanceEntry)

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsPerformance* GetParentObject() const
  {
    return mPerformance;
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

protected:
  nsRefPtr<nsPerformance> mPerformance;
  nsString mName;
  nsString mEntryType;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PerformanceEntry_h___ */
