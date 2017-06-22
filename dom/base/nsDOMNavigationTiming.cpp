/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMNavigationTiming.h"

#include "GeckoProfiler.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIScriptSecurityManager.h"
#include "prtime.h"
#include "nsIURI.h"
#include "nsPrintfCString.h"
#include "mozilla/dom/PerformanceNavigation.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

nsDOMNavigationTiming::nsDOMNavigationTiming(nsDocShell* aDocShell)
{
  Clear();

  mDocShell = aDocShell;
}

nsDOMNavigationTiming::~nsDOMNavigationTiming()
{
}

void
nsDOMNavigationTiming::Clear()
{
  mNavigationType = TYPE_RESERVED;
  mNavigationStartHighRes = 0;
  mBeforeUnloadStart = 0;
  mUnloadStart = 0;
  mUnloadEnd = 0;
  mLoadEventStart = 0;
  mLoadEventEnd = 0;
  mDOMLoading = 0;
  mDOMInteractive = 0;
  mDOMContentLoadedEventStart = 0;
  mDOMContentLoadedEventEnd = 0;
  mDOMComplete = 0;

  mLoadEventStartSet = false;
  mLoadEventEndSet = false;
  mDOMLoadingSet = false;
  mDOMInteractiveSet = false;
  mDOMContentLoadedEventStartSet = false;
  mDOMContentLoadedEventEndSet = false;
  mDOMCompleteSet = false;
  mDocShellHasBeenActiveSinceNavigationStart = false;
}

DOMTimeMilliSec
nsDOMNavigationTiming::TimeStampToDOM(TimeStamp aStamp) const
{
  if (aStamp.IsNull()) {
    return 0;
  }

  TimeDuration duration = aStamp - mNavigationStartTimeStamp;
  return GetNavigationStart() + static_cast<int64_t>(duration.ToMilliseconds());
}

DOMTimeMilliSec nsDOMNavigationTiming::DurationFromStart()
{
  return TimeStampToDOM(TimeStamp::Now());
}

void
nsDOMNavigationTiming::NotifyNavigationStart(DocShellState aDocShellState)
{
  mNavigationStartHighRes = (double)PR_Now() / PR_USEC_PER_MSEC;
  mNavigationStartTimeStamp = TimeStamp::Now();
  mDocShellHasBeenActiveSinceNavigationStart = (aDocShellState == DocShellState::eActive);
  profiler_add_marker("Navigation::Start");
}

void
nsDOMNavigationTiming::NotifyFetchStart(nsIURI* aURI, Type aNavigationType)
{
  mNavigationType = aNavigationType;
  // At the unload event time we don't really know the loading uri.
  // Need it for later check for unload timing access.
  mLoadedURI = aURI;
}

void
nsDOMNavigationTiming::NotifyBeforeUnload()
{
  mBeforeUnloadStart = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyUnloadAccepted(nsIURI* aOldURI)
{
  mUnloadStart = mBeforeUnloadStart;
  mUnloadedURI = aOldURI;
}

void
nsDOMNavigationTiming::NotifyUnloadEventStart()
{
  mUnloadStart = DurationFromStart();
  profiler_tracing("Navigation", "Unload", TRACING_INTERVAL_START);
}

void
nsDOMNavigationTiming::NotifyUnloadEventEnd()
{
  mUnloadEnd = DurationFromStart();
  profiler_tracing("Navigation", "Unload", TRACING_INTERVAL_END);
}

void
nsDOMNavigationTiming::NotifyLoadEventStart()
{
  if (!mLoadEventStartSet) {
    mLoadEventStart = DurationFromStart();
    mLoadEventStartSet = true;

    profiler_tracing("Navigation", "Load", TRACING_INTERVAL_START);

    if (IsTopLevelContentDocument()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_START_MS,
                                     mNavigationStartTimeStamp);
    }
  }
}

void
nsDOMNavigationTiming::NotifyLoadEventEnd()
{
  if (!mLoadEventEndSet) {
    mLoadEventEnd = DurationFromStart();
    mLoadEventEndSet = true;

    profiler_tracing("Navigation", "Load", TRACING_INTERVAL_END);

    if (IsTopLevelContentDocument()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_END_MS,
                                     mNavigationStartTimeStamp);
    }
  }
}

void
nsDOMNavigationTiming::SetDOMLoadingTimeStamp(nsIURI* aURI, TimeStamp aValue)
{
  if (!mDOMLoadingSet) {
    mLoadedURI = aURI;
    mDOMLoading = TimeStampToDOM(aValue);
    mDOMLoadingSet = true;
  }
}

void
nsDOMNavigationTiming::NotifyDOMLoading(nsIURI* aURI)
{
  if (!mDOMLoadingSet) {
    mLoadedURI = aURI;
    mDOMLoading = DurationFromStart();
    mDOMLoadingSet = true;

    profiler_add_marker("Navigation::DOMLoading");

    if (IsTopLevelContentDocument()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_LOADING_MS,
                                     mNavigationStartTimeStamp);
    }
  }
}

void
nsDOMNavigationTiming::NotifyDOMInteractive(nsIURI* aURI)
{
  if (!mDOMInteractiveSet) {
    mLoadedURI = aURI;
    mDOMInteractive = DurationFromStart();
    mDOMInteractiveSet = true;

    profiler_add_marker("Navigation::DOMInteractive");

    if (IsTopLevelContentDocument()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_INTERACTIVE_MS,
                                     mNavigationStartTimeStamp);
    }
  }
}

void
nsDOMNavigationTiming::NotifyDOMComplete(nsIURI* aURI)
{
  if (!mDOMCompleteSet) {
    mLoadedURI = aURI;
    mDOMComplete = DurationFromStart();
    mDOMCompleteSet = true;

    profiler_add_marker("Navigation::DOMComplete");

    if (IsTopLevelContentDocument()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_COMPLETE_MS,
                                     mNavigationStartTimeStamp);
    }
  }
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedStart(nsIURI* aURI)
{
  if (!mDOMContentLoadedEventStartSet) {
    mLoadedURI = aURI;
    mDOMContentLoadedEventStart = DurationFromStart();
    mDOMContentLoadedEventStartSet = true;

    profiler_tracing("Navigation", "DOMContentLoaded", TRACING_INTERVAL_START);

    if (IsTopLevelContentDocument()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_CONTENT_LOADED_START_MS,
                                     mNavigationStartTimeStamp);
    }
  }
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedEnd(nsIURI* aURI)
{
  if (!mDOMContentLoadedEventEndSet) {
    mLoadedURI = aURI;
    mDOMContentLoadedEventEnd = DurationFromStart();
    mDOMContentLoadedEventEndSet = true;

    profiler_tracing("Navigation", "DOMContentLoaded", TRACING_INTERVAL_END);

    if (IsTopLevelContentDocument()) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_CONTENT_LOADED_END_MS,
                                     mNavigationStartTimeStamp);
    }
  }
}

void
nsDOMNavigationTiming::NotifyNonBlankPaintForRootContentDocument()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mNavigationStartTimeStamp.IsNull());

  if (!mNonBlankPaintTimeStamp.IsNull()) {
    return;
  }

  mNonBlankPaintTimeStamp = TimeStamp::Now();
  TimeDuration elapsed = mNonBlankPaintTimeStamp - mNavigationStartTimeStamp;

  if (profiler_is_active()) {
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker("Non-blank paint after %dms for URL %s, %s",
                           int(elapsed.ToMilliseconds()), spec.get(),
                           mDocShellHasBeenActiveSinceNavigationStart ? "foreground tab" : "this tab was inactive some of the time between navigation start and first non-blank paint");
    profiler_add_marker(marker.get());
  }

  if (mDocShellHasBeenActiveSinceNavigationStart) {
    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_NON_BLANK_PAINT_MS,
                                   mNavigationStartTimeStamp,
                                   mNonBlankPaintTimeStamp);
  }
}

void
nsDOMNavigationTiming::NotifyDocShellStateChanged(DocShellState aDocShellState)
{
  mDocShellHasBeenActiveSinceNavigationStart &=
    (aDocShellState == DocShellState::eActive);
}

DOMTimeMilliSec
nsDOMNavigationTiming::GetUnloadEventStart()
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadStart;
  }
  return 0;
}

DOMTimeMilliSec
nsDOMNavigationTiming::GetUnloadEventEnd()
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadEnd;
  }
  return 0;
}

bool
nsDOMNavigationTiming::IsTopLevelContentDocument() const
{
  if (!mDocShell) {
    return false;
  }
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  Unused << mDocShell->GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
  if (rootItem.get() != static_cast<nsIDocShellTreeItem*>(mDocShell.get())) {
    return false;
  }
  return rootItem->ItemType() == nsIDocShellTreeItem::typeContent;
}
