/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceServerTiming.h"

#include "mozilla/dom/PerformanceServerTimingBinding.h"
#include "nsITimedChannel.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PerformanceServerTiming, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PerformanceServerTiming)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PerformanceServerTiming)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceServerTiming)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
PerformanceServerTiming::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::PerformanceServerTiming_Binding::Wrap(aCx, this, aGivenProto);
}

void
PerformanceServerTiming::GetName(nsAString& aName) const
{
  aName.Truncate();

  if (!mServerTiming) {
    return;
  }

  nsAutoCString name;
  if (NS_WARN_IF(NS_FAILED(mServerTiming->GetName(name)))) {
    return;
  }

  aName.Assign(NS_ConvertUTF8toUTF16(name));
}

DOMHighResTimeStamp
PerformanceServerTiming::Duration() const
{
  if (!mServerTiming) {
    return 0;
  }

  double duration = 0;
  if (NS_WARN_IF(NS_FAILED(mServerTiming->GetDuration(&duration)))) {
    return 0;
  }

  return duration;
}

void
PerformanceServerTiming::GetDescription(nsAString& aDescription) const
{
  if (!mServerTiming) {
    return;
  }

  nsAutoCString description;
  if (NS_WARN_IF(NS_FAILED(mServerTiming->GetDescription(description)))) {
    return;
  }

  aDescription.Assign(NS_ConvertUTF8toUTF16(description));
}

} // dom namespace
} // mozilla namespace
