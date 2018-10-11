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
#include "ProfilerMarkerPayload.h"

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

// static
void
nsDOMNavigationTiming::TTITimeoutCallback(nsITimer* aTimer, void *aClosure)
{
  nsDOMNavigationTiming* self = static_cast<nsDOMNavigationTiming*>(aClosure);
  self->TTITimeout(aTimer);
}

// Return the max of aT1 and aT2, or the lower of the two if there's more
// than Nms (the window size) between them.  In other words, the window
// starts at the lower of aT1 and aT2, and we only want to respect
// timestamps within the window (and pick the max of those).
//
// This approach handles the edge case of a late wakeup: where there was
// more than Nms after one (of aT1 or aT2) without the other, but the other
// happened after Nms and before we woke up.  For example, if aT1 was 10
// seconds after aT2, but we woke up late (after aT1) we don't want to
// return aT1 if the window is 5 seconds.
static const TimeStamp&
MaxWithinWindowBeginningAtMin(const TimeStamp& aT1, const TimeStamp& aT2,
                              const TimeDuration& aWindowSize)
{
  if (aT2.IsNull()) {
    return aT1;
  } else if (aT1.IsNull()) {
    return aT2;
  }
  if (aT1 > aT2) {
    if ((aT1 - aT2) > aWindowSize) {
      return aT2;
    }
    return aT1;
  }
  if ((aT2 - aT1) > aWindowSize) {
    return aT1;
  }
  return aT2;
}

#define TTI_WINDOW_SIZE_MS (5 * 1000)

void
nsDOMNavigationTiming::TTITimeout(nsITimer* aTimer)
{
  // Check TTI: see if it's been 5 seconds since the last Long Task
  TimeStamp now = TimeStamp::Now();
  MOZ_RELEASE_ASSERT(!mNonBlankPaint.IsNull(), "TTI timeout with no non-blank-paint?");

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  TimeStamp lastLongTaskEnded;
  mainThread->GetLastLongNonIdleTaskEnd(&lastLongTaskEnded);
  if (!lastLongTaskEnded.IsNull()) {
    TimeDuration delta = now - lastLongTaskEnded;
    if (delta.ToMilliseconds() < TTI_WINDOW_SIZE_MS) {
      // Less than 5 seconds since the last long task.  Schedule another check
      aTimer->InitWithNamedFuncCallback(TTITimeoutCallback, this, TTI_WINDOW_SIZE_MS,
                                        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                         "nsDOMNavigationTiming::TTITimeout");
      return;
    }
  }
  // To correctly implement TTI/TTFI as proposed, we'd need to use
  // FirstContentfulPaint (FCP, which we have not yet implemented) instead
  // of FirstNonBlankPaing (FNBP) to start at, and not fire it until there
  // are no more than 2 network loads.  By the proposed definition, without
  // that we're closer to TimeToFirstInteractive.

  // XXX check number of network loads, and if > 2 mark to check if loads
  // decreases to 2 (or record that point and let the normal timer here
  // handle it)

  // TTI has occurred!  TTI is either FCP (if there are no longtasks and no
  // DCLEnd in the window that starts at FCP), or at the end of the last
  // Long Task or DOMContentLoadedEnd (whichever is later).

  if (mTTFI.IsNull()) {
    mTTI = MaxWithinWindowBeginningAtMin(lastLongTaskEnded, mDOMContentLoadedEventEnd,
                                         TimeDuration::FromMilliseconds(TTI_WINDOW_SIZE_MS));
    if (mTTFI.IsNull()) {
      mTTFI = mNonBlankPaint;
    }
  }
  // XXX Implement TTI via check number of network loads, and if > 2 mark
  // to check if loads decreases to 2 (or record that point and let the
  // normal timer here handle it)

  mTTITimer = nullptr;

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    TimeDuration elapsed = mTTFI - mNavigationStart;
    TimeDuration elapsedLongTask = lastLongTaskEnded.IsNull() ? 0 : lastLongTaskEnded - mNavigationStart;
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker("TTFI after %dms (LongTask after %dms) for URL %s",
                           int(elapsed.ToMilliseconds()),
                           int(elapsedLongTask.ToMilliseconds()),spec.get());

    profiler_add_marker(
      "TTI", MakeUnique<UserTimingMarkerPayload>(NS_ConvertASCIItoUTF16(marker), mTTFI));
  }
#endif
  return;
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

  if (!mTTITimer) {
    mTTITimer = NS_NewTimer();
  }

  // TTI is first checked 5 seconds after the FCP (non-blank-paint is very close to FCP).
  mTTITimer->InitWithNamedFuncCallback(TTITimeoutCallback, this, TTI_WINDOW_SIZE_MS,
                                       nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                       "nsDOMNavigationTiming::TTITimeout");

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
  // todo: if you intend to update CheckSameOriginURI to log the error to the
  // console you also need to update the 'aFromPrivateWindow' argument.
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadStart;
  }
  return mozilla::TimeStamp();
}

mozilla::TimeStamp
nsDOMNavigationTiming::GetUnloadEventEndTimeStamp() const
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  // todo: if you intend to update CheckSameOriginURI to log the error to the
  // console you also need to update the 'aFromPrivateWindow' argument.
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false, false);
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
