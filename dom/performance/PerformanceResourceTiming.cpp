/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceResourceTiming.h"
#include "mozilla/dom/PerformanceResourceTimingBinding.h"
#include "nsNetUtil.h"
#include "nsArrayUtils.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(PerformanceResourceTiming, PerformanceEntry,
                                   mPerformance)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PerformanceResourceTiming,
                                               PerformanceEntry)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceResourceTiming)
NS_INTERFACE_MAP_END_INHERITING(PerformanceEntry)

NS_IMPL_ADDREF_INHERITED(PerformanceResourceTiming, PerformanceEntry)
NS_IMPL_RELEASE_INHERITED(PerformanceResourceTiming, PerformanceEntry)

PerformanceResourceTiming::PerformanceResourceTiming(
    UniquePtr<PerformanceTimingData>&& aPerformanceTiming,
    Performance* aPerformance, const nsAString& aName)
    : PerformanceEntry(aPerformance->GetParentObject(), aName, u"resource"_ns),
      mTimingData(std::move(aPerformanceTiming)),
      mPerformance(aPerformance) {
  MOZ_RELEASE_ASSERT(mTimingData);
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
  if (NS_IsMainThread()) {
    // Used to check if an addon content script has access to this timing.
    // We don't need it in workers, and ignore mOriginalURI if null.
    NS_NewURI(getter_AddRefs(mOriginalURI), aName);
  }
}

PerformanceResourceTiming::~PerformanceResourceTiming() = default;

DOMHighResTimeStamp PerformanceResourceTiming::FetchStart() const {
  if (mTimingData->TimingAllowed()) {
    return mTimingData->FetchStartHighRes(mPerformance);
  }
  return StartTime();
}

DOMHighResTimeStamp PerformanceResourceTiming::StartTime() const {
  // Force the start time to be the earliest of:
  //  - RedirectStart
  //  - WorkerStart
  //  - AsyncOpen
  // Ignore zero values.  The RedirectStart and WorkerStart values
  // can come from earlier redirected channels prior to the AsyncOpen
  // time being recorded.
  if (mCachedStartTime.isNothing()) {
    DOMHighResTimeStamp redirect =
        mTimingData->RedirectStartHighRes(mPerformance);
    redirect = redirect ? redirect : DBL_MAX;

    DOMHighResTimeStamp worker = mTimingData->WorkerStartHighRes(mPerformance);
    worker = worker ? worker : DBL_MAX;

    DOMHighResTimeStamp asyncOpen = mTimingData->AsyncOpenHighRes(mPerformance);

    mCachedStartTime.emplace(std::min(asyncOpen, std::min(redirect, worker)));
  }
  return mCachedStartTime.value();
}

JSObject* PerformanceResourceTiming::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PerformanceResourceTiming_Binding::Wrap(aCx, this, aGivenProto);
}

size_t PerformanceResourceTiming::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

size_t PerformanceResourceTiming::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  return PerformanceEntry::SizeOfExcludingThis(aMallocSizeOf) +
         mInitiatorType.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mTimingData->NextHopProtocol().SizeOfExcludingThisIfUnshared(
             aMallocSizeOf);
}

void PerformanceResourceTiming::GetServerTiming(
    nsTArray<RefPtr<PerformanceServerTiming>>& aRetval,
    nsIPrincipal& aSubjectPrincipal) {
  aRetval.Clear();
  if (!TimingAllowedForCaller(aSubjectPrincipal)) {
    return;
  }

  nsTArray<nsCOMPtr<nsIServerTiming>> serverTimingArray =
      mTimingData->GetServerTiming();
  uint32_t length = serverTimingArray.Length();
  for (uint32_t index = 0; index < length; ++index) {
    nsCOMPtr<nsIServerTiming> serverTiming = serverTimingArray.ElementAt(index);
    MOZ_ASSERT(serverTiming);

    aRetval.AppendElement(
        new PerformanceServerTiming(GetParentObject(), serverTiming));
  }
}

bool PerformanceResourceTiming::TimingAllowedForCaller(
    nsIPrincipal& aCaller) const {
  if (mTimingData->TimingAllowed()) {
    return true;
  }

  // Check if the addon has permission to access the cross-origin resource.
  return mOriginalURI &&
         BasePrincipal::Cast(&aCaller)->AddonAllowsLoad(mOriginalURI);
}

bool PerformanceResourceTiming::ReportRedirectForCaller(
    nsIPrincipal& aCaller, bool aEnsureSameOriginAndIgnoreTAO) const {
  if (mTimingData->ShouldReportCrossOriginRedirect(
          aEnsureSameOriginAndIgnoreTAO)) {
    return true;
  }

  // Only report cross-origin redirect if the addon has <all_urls> permission.
  return BasePrincipal::Cast(&aCaller)->AddonHasPermission(
      nsGkAtoms::all_urlsPermission);
}

RenderBlockingStatusType PerformanceResourceTiming::RenderBlockingStatus()
    const {
  return mTimingData->RenderBlockingStatus();
}
