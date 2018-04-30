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
#include "nsHttp.h"
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

  mBeforeUnloadStart = TimeStamp();
  mUnloadStart = TimeStamp();
  mUnloadEnd = TimeStamp();
  mLoadEventStart = TimeStamp();
  mLoadEventEnd = TimeStamp();
  mDOMLoading = TimeStamp();
  mDOMInteractive = TimeStamp();
  mDOMContentLoadedEventStart = TimeStamp();
  mDOMContentLoadedEventEnd = TimeStamp();
  mDOMComplete = TimeStamp();

  mDocShellHasBeenActiveSinceNavigationStart = false;
}

DOMTimeMilliSec
nsDOMNavigationTiming::TimeStampToDOM(TimeStamp aStamp) const
{
  if (aStamp.IsNull()) {
    return 0;
  }

  TimeDuration duration = aStamp - mNavigationStart;
  return GetNavigationStart() + static_cast<int64_t>(duration.ToMilliseconds());
}

void
nsDOMNavigationTiming::NotifyNavigationStart(DocShellState aDocShellState)
{
  mNavigationStartHighRes = (double)PR_Now() / PR_USEC_PER_MSEC;
  mNavigationStart = TimeStamp::Now();
  mDocShellHasBeenActiveSinceNavigationStart = (aDocShellState == DocShellState::eActive);
  PROFILER_ADD_MARKER("Navigation::Start");
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
  mBeforeUnloadStart = TimeStamp::Now();
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
  mUnloadStart = TimeStamp::Now();
  PROFILER_TRACING("Navigation", "Unload", TRACING_INTERVAL_START);
}

void
nsDOMNavigationTiming::NotifyUnloadEventEnd()
{
  mUnloadEnd = TimeStamp::Now();
  PROFILER_TRACING("Navigation", "Unload", TRACING_INTERVAL_END);
}

void
nsDOMNavigationTiming::NotifyLoadEventStart()
{
  if (!mLoadEventStart.IsNull()) {
    return;
  }
  mLoadEventStart = TimeStamp::Now();

  PROFILER_TRACING("Navigation", "Load", TRACING_INTERVAL_START);

  if (IsTopLevelContentDocumentInContentProcess()) {
    TimeStamp now = TimeStamp::Now();

    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_START_MS,
                                   mNavigationStart,
                                   now);

    if (mDocShellHasBeenActiveSinceNavigationStart) {
      if (net::nsHttp::IsBeforeLastActiveTabLoadOptimization(mNavigationStart)) {
        Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_START_ACTIVE_NETOPT_MS,
                                       mNavigationStart,
                                       now);
      } else {
        Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_START_ACTIVE_MS,
                                       mNavigationStart,
                                       now);
      }
    }
  }
}

void
nsDOMNavigationTiming::NotifyLoadEventEnd()
{
  if (!mLoadEventEnd.IsNull()) {
    return;
  }
  mLoadEventEnd = TimeStamp::Now();

  PROFILER_TRACING("Navigation", "Load", TRACING_INTERVAL_END);

  if (IsTopLevelContentDocumentInContentProcess()) {
    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_END_MS,
                                   mNavigationStart);
  }
}

void
nsDOMNavigationTiming::SetDOMLoadingTimeStamp(nsIURI* aURI, TimeStamp aValue)
{
  if (!mDOMLoading.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMLoading = aValue;
}

void
nsDOMNavigationTiming::NotifyDOMLoading(nsIURI* aURI)
{
  if (!mDOMLoading.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMLoading = TimeStamp::Now();

  PROFILER_ADD_MARKER("Navigation::DOMLoading");
}

void
nsDOMNavigationTiming::NotifyDOMInteractive(nsIURI* aURI)
{
  if (!mDOMInteractive.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMInteractive = TimeStamp::Now();

  PROFILER_ADD_MARKER("Navigation::DOMInteractive");
}

void
nsDOMNavigationTiming::NotifyDOMComplete(nsIURI* aURI)
{
  if (!mDOMComplete.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMComplete = TimeStamp::Now();

  PROFILER_ADD_MARKER("Navigation::DOMComplete");
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedStart(nsIURI* aURI)
{
  if (!mDOMContentLoadedEventStart.IsNull()) {
    return;
  }

  mLoadedURI = aURI;
  mDOMContentLoadedEventStart = TimeStamp::Now();

  PROFILER_TRACING("Navigation", "DOMContentLoaded", TRACING_INTERVAL_START);

  if (IsTopLevelContentDocumentInContentProcess()) {
    TimeStamp now = TimeStamp::Now();

    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_CONTENT_LOADED_START_MS,
                                   mNavigationStart,
                                   now);

    if (mDocShellHasBeenActiveSinceNavigationStart) {
      if (net::nsHttp::IsBeforeLastActiveTabLoadOptimization(mNavigationStart)) {
        Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_CONTENT_LOADED_START_ACTIVE_NETOPT_MS,
                                       mNavigationStart,
                                       now);
      } else {
        Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_CONTENT_LOADED_START_ACTIVE_MS,
                                       mNavigationStart,
                                       now);
      }
    }
  }
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedEnd(nsIURI* aURI)
{
  if (!mDOMContentLoadedEventEnd.IsNull()) {
    return;
  }

  mLoadedURI = aURI;
  mDOMContentLoadedEventEnd = TimeStamp::Now();

  PROFILER_TRACING("Navigation", "DOMContentLoaded", TRACING_INTERVAL_END);

  if (IsTopLevelContentDocumentInContentProcess()) {
    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_CONTENT_LOADED_END_MS,
                                   mNavigationStart);
  }
}

void
nsDOMNavigationTiming::NotifyNonBlankPaintForRootContentDocument()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mNavigationStart.IsNull());

  if (!mNonBlankPaint.IsNull()) {
    return;
  }

  mNonBlankPaint = TimeStamp::Now();

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    TimeDuration elapsed = mNonBlankPaint - mNavigationStart;
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker("Non-blank paint after %dms for URL %s, %s",
                           int(elapsed.ToMilliseconds()), spec.get(),
                           mDocShellHasBeenActiveSinceNavigationStart ? "foreground tab" : "this tab was inactive some of the time between navigation start and first non-blank paint");
    profiler_add_marker(marker.get());
  }
#endif

  if (mDocShellHasBeenActiveSinceNavigationStart) {
    if (net::nsHttp::IsBeforeLastActiveTabLoadOptimization(mNavigationStart)) {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_NON_BLANK_PAINT_NETOPT_MS,
                                     mNavigationStart,
                                     mNonBlankPaint);
    } else {
      Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_NON_BLANK_PAINT_NO_NETOPT_MS,
                                     mNavigationStart,
                                     mNonBlankPaint);
    }

    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_NON_BLANK_PAINT_MS,
                                   mNavigationStart,
                                   mNonBlankPaint);
  }
}

void
nsDOMNavigationTiming::NotifyDOMContentFlushedForRootContentDocument()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mNavigationStart.IsNull());

  if (!mDOMContentFlushed.IsNull()) {
    return;
  }

  mDOMContentFlushed = TimeStamp::Now();

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    TimeDuration elapsed = mDOMContentFlushed - mNavigationStart;
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker("DOMContentFlushed after %dms for URL %s, %s",
                           int(elapsed.ToMilliseconds()), spec.get(),
                           mDocShellHasBeenActiveSinceNavigationStart ? "foreground tab" : "this tab was inactive some of the time between navigation start and DOMContentFlushed");
    profiler_add_marker(marker.get());
  }
#endif
}

void
nsDOMNavigationTiming::NotifyDocShellStateChanged(DocShellState aDocShellState)
{
  mDocShellHasBeenActiveSinceNavigationStart &=
    (aDocShellState == DocShellState::eActive);
}

mozilla::TimeStamp
nsDOMNavigationTiming::GetUnloadEventStartTimeStamp() const
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadStart;
  }
  return mozilla::TimeStamp();
}

mozilla::TimeStamp
nsDOMNavigationTiming::GetUnloadEventEndTimeStamp() const
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadEnd;
  }
  return mozilla::TimeStamp();
}

bool
nsDOMNavigationTiming::IsTopLevelContentDocumentInContentProcess() const
{
  if (!mDocShell) {
    return false;
  }
  if (!XRE_IsContentProcess()) {
    return false;
  }
  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  Unused << mDocShell->GetSameTypeRootTreeItem(getter_AddRefs(rootItem));
  if (rootItem.get() != static_cast<nsIDocShellTreeItem*>(mDocShell.get())) {
    return false;
  }
  return rootItem->ItemType() == nsIDocShellTreeItem::typeContent;
}
