/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceNavigation.h"
#include "PerformanceTiming.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceNavigation, mPerformance)

PerformanceNavigation::PerformanceNavigation(Performance* aPerformance)
    : mPerformance(aPerformance) {
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
}

PerformanceNavigation::~PerformanceNavigation() = default;

JSObject* PerformanceNavigation::WrapObject(JSContext* cx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return PerformanceNavigation_Binding::Wrap(cx, this, aGivenProto);
}

uint16_t PerformanceNavigation::RedirectCount() const {
  return GetPerformanceTiming()->Data()->GetRedirectCount();
}

}  // namespace mozilla::dom
