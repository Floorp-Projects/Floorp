/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceNavigation_h
#define mozilla_dom_PerformanceNavigation_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/Performance.h"
#include "nsDOMNavigationTiming.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

// Script "performance.navigation" object
class PerformanceNavigation final : public nsWrapperCache
{
public:
  enum PerformanceNavigationType {
    TYPE_NAVIGATE = 0,
    TYPE_RELOAD = 1,
    TYPE_BACK_FORWARD = 2,
    TYPE_RESERVED = 255,
  };

  explicit PerformanceNavigation(Performance* aPerformance);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PerformanceNavigation)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PerformanceNavigation)

  nsDOMNavigationTiming* GetDOMTiming() const
  {
    return mPerformance->GetDOMTiming();
  }

  PerformanceTiming* GetPerformanceTiming() const
  {
    return mPerformance->Timing();
  }

  Performance* GetParentObject() const
  {
    return mPerformance;
  }

  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // PerformanceNavigation WebIDL methods
  uint16_t Type() const
  {
    return GetDOMTiming()->GetType();
  }

  uint16_t RedirectCount() const;

private:
  ~PerformanceNavigation();
  RefPtr<Performance> mPerformance;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PerformanceNavigation_h
