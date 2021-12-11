/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceMark.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/PerformanceMarkBinding.h"

using namespace mozilla::dom;

PerformanceMark::PerformanceMark(nsISupports* aParent, const nsAString& aName,
                                 DOMHighResTimeStamp aStartTime)
    : PerformanceEntry(aParent, aName, u"mark"_ns), mStartTime(aStartTime) {}

PerformanceMark::~PerformanceMark() = default;

JSObject* PerformanceMark::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return PerformanceMark_Binding::Wrap(aCx, this, aGivenProto);
}

size_t PerformanceMark::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}
