/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMNavigationTiming.h"

#include "GeckoProfiler.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/PerformanceNavigation.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsHttp.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsPrintfCString.h"
#include "prtime.h"
#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif

using namespace mozilla;

namespace mozilla {

LazyLogModule gPageLoadLog("PageLoad");
#define PAGELOAD_LOG(args) MOZ_LOG(gPageLoadLog, LogLevel::Debug, args)
#define PAGELOAD_LOG_ENABLED() MOZ_LOG_TEST(gPageLoadLog, LogLevel::Error)

}  // namespace mozilla

nsDOMNavigationTiming::nsDOMNavigationTiming(nsDocShell* aDocShell) {
  Clear();

  mDocShell = aDocShell;
}

nsDOMNavigationTiming::~nsDOMNavigationTiming() = default;

void nsDOMNavigationTiming::Clear() {
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
  mContentfulPaint = TimeStamp();
  mNonBlankPaint = TimeStamp();

  mDocShellHasBeenActiveSinceNavigationStart = false;
}

void nsDOMNavigationTiming::Anonymize(nsIURI* aFinalURI) {
  mLoadedURI = aFinalURI;
  mUnloadedURI = nullptr;
  mBeforeUnloadStart = TimeStamp();
  mUnloadStart = TimeStamp();
  mUnloadEnd = TimeStamp();
}

DOMTimeMilliSec nsDOMNavigationTiming::TimeStampToDOM(TimeStamp aStamp) const {
  if (aStamp.IsNull()) {
    return 0;
  }

  TimeDuration duration = aStamp - mNavigationStart;
  return GetNavigationStart() + static_cast<int64_t>(duration.ToMilliseconds());
}

void nsDOMNavigationTiming::NotifyNavigationStart(
    DocShellState aDocShellState) {
  mNavigationStartHighRes = (double)PR_Now() / PR_USEC_PER_MSEC;
  mNavigationStart = TimeStamp::Now();
  mDocShellHasBeenActiveSinceNavigationStart =
      (aDocShellState == DocShellState::eActive);
  PROFILER_ADD_MARKER("Navigation::Start", DOM);
}

void nsDOMNavigationTiming::NotifyFetchStart(nsIURI* aURI,
                                             Type aNavigationType) {
  mNavigationType = aNavigationType;
  // At the unload event time we don't really know the loading uri.
  // Need it for later check for unload timing access.
  mLoadedURI = aURI;
}

void nsDOMNavigationTiming::NotifyRestoreStart() {
  mNavigationType = TYPE_BACK_FORWARD;
}

void nsDOMNavigationTiming::NotifyBeforeUnload() {
  mBeforeUnloadStart = TimeStamp::Now();
}

void nsDOMNavigationTiming::NotifyUnloadAccepted(nsIURI* aOldURI) {
  mUnloadStart = mBeforeUnloadStart;
  mUnloadedURI = aOldURI;
}

void nsDOMNavigationTiming::NotifyUnloadEventStart() {
  mUnloadStart = TimeStamp::Now();
  PROFILER_TRACING_MARKER_DOCSHELL("Navigation", "Unload", NETWORK,
                                   TRACING_INTERVAL_START, mDocShell);
}

void nsDOMNavigationTiming::NotifyUnloadEventEnd() {
  mUnloadEnd = TimeStamp::Now();
  PROFILER_TRACING_MARKER_DOCSHELL("Navigation", "Unload", NETWORK,
                                   TRACING_INTERVAL_END, mDocShell);
}

void nsDOMNavigationTiming::NotifyLoadEventStart() {
  if (!mLoadEventStart.IsNull()) {
    return;
  }
  mLoadEventStart = TimeStamp::Now();

  PROFILER_TRACING_MARKER_DOCSHELL("Navigation", "Load", NETWORK,
                                   TRACING_INTERVAL_START, mDocShell);

  if (IsTopLevelContentDocumentInContentProcess()) {
    TimeStamp now = TimeStamp::Now();

    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_START_MS,
                                   mNavigationStart, now);

    if (mDocShellHasBeenActiveSinceNavigationStart) {
      if (net::nsHttp::IsBeforeLastActiveTabLoadOptimization(
              mNavigationStart)) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::TIME_TO_LOAD_EVENT_START_ACTIVE_NETOPT_MS,
            mNavigationStart, now);
      } else {
        Telemetry::AccumulateTimeDelta(
            Telemetry::TIME_TO_LOAD_EVENT_START_ACTIVE_MS, mNavigationStart,
            now);
      }
    }
  }
}

void nsDOMNavigationTiming::NotifyLoadEventEnd() {
  if (!mLoadEventEnd.IsNull()) {
    return;
  }
  mLoadEventEnd = TimeStamp::Now();

  PROFILER_TRACING_MARKER_DOCSHELL("Navigation", "Load", NETWORK,
                                   TRACING_INTERVAL_END, mDocShell);

  if (IsTopLevelContentDocumentInContentProcess()) {
#ifdef MOZ_GECKO_PROFILER
    if (profiler_can_accept_markers() || PAGELOAD_LOG_ENABLED()) {
      TimeDuration elapsed = mLoadEventEnd - mNavigationStart;
      TimeDuration duration = mLoadEventEnd - mLoadEventStart;
      nsAutoCString spec;
      if (mLoadedURI) {
        mLoadedURI->GetSpec(spec);
      }
      nsPrintfCString marker(
          "Document %s loaded after %dms, load event duration %dms", spec.get(),
          int(elapsed.ToMilliseconds()), int(duration.ToMilliseconds()));
      PAGELOAD_LOG(("%s", marker.get()));
      PROFILER_ADD_MARKER_WITH_PAYLOAD(
          "DocumentLoad", DOM, TextMarkerPayload,
          (marker, mNavigationStart, mLoadEventEnd,
           profiler_get_inner_window_id_from_docshell(mDocShell)));
    }
#endif
    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_LOAD_EVENT_END_MS,
                                   mNavigationStart);
  }
}

void nsDOMNavigationTiming::SetDOMLoadingTimeStamp(nsIURI* aURI,
                                                   TimeStamp aValue) {
  if (!mDOMLoading.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMLoading = aValue;
}

void nsDOMNavigationTiming::NotifyDOMLoading(nsIURI* aURI) {
  if (!mDOMLoading.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMLoading = TimeStamp::Now();

  PROFILER_ADD_MARKER("Navigation::DOMLoading", DOM);
}

void nsDOMNavigationTiming::NotifyDOMInteractive(nsIURI* aURI) {
  if (!mDOMInteractive.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMInteractive = TimeStamp::Now();

  PROFILER_ADD_MARKER("Navigation::DOMInteractive", DOM);
}

void nsDOMNavigationTiming::NotifyDOMComplete(nsIURI* aURI) {
  if (!mDOMComplete.IsNull()) {
    return;
  }
  mLoadedURI = aURI;
  mDOMComplete = TimeStamp::Now();

  PROFILER_ADD_MARKER("Navigation::DOMComplete", DOM);
}

void nsDOMNavigationTiming::NotifyDOMContentLoadedStart(nsIURI* aURI) {
  if (!mDOMContentLoadedEventStart.IsNull()) {
    return;
  }

  mLoadedURI = aURI;
  mDOMContentLoadedEventStart = TimeStamp::Now();

  PROFILER_TRACING_MARKER_DOCSHELL("Navigation", "DOMContentLoaded", NETWORK,
                                   TRACING_INTERVAL_START, mDocShell);

  if (IsTopLevelContentDocumentInContentProcess()) {
    TimeStamp now = TimeStamp::Now();

    Telemetry::AccumulateTimeDelta(
        Telemetry::TIME_TO_DOM_CONTENT_LOADED_START_MS, mNavigationStart, now);

    if (mDocShellHasBeenActiveSinceNavigationStart) {
      if (net::nsHttp::IsBeforeLastActiveTabLoadOptimization(
              mNavigationStart)) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::TIME_TO_DOM_CONTENT_LOADED_START_ACTIVE_NETOPT_MS,
            mNavigationStart, now);
      } else {
        Telemetry::AccumulateTimeDelta(
            Telemetry::TIME_TO_DOM_CONTENT_LOADED_START_ACTIVE_MS,
            mNavigationStart, now);
      }
    }
  }
}

void nsDOMNavigationTiming::NotifyDOMContentLoadedEnd(nsIURI* aURI) {
  if (!mDOMContentLoadedEventEnd.IsNull()) {
    return;
  }

  mLoadedURI = aURI;
  mDOMContentLoadedEventEnd = TimeStamp::Now();

  PROFILER_TRACING_MARKER_DOCSHELL("Navigation", "DOMContentLoaded", NETWORK,
                                   TRACING_INTERVAL_END, mDocShell);

  if (IsTopLevelContentDocumentInContentProcess()) {
    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_DOM_CONTENT_LOADED_END_MS,
                                   mNavigationStart);
  }
}

// static
void nsDOMNavigationTiming::TTITimeoutCallback(nsITimer* aTimer,
                                               void* aClosure) {
  nsDOMNavigationTiming* self = static_cast<nsDOMNavigationTiming*>(aClosure);
  self->TTITimeout(aTimer);
}

#define TTI_WINDOW_SIZE_MS (5 * 1000)

void nsDOMNavigationTiming::TTITimeout(nsITimer* aTimer) {
  // Check TTI: see if it's been 5 seconds since the last Long Task
  TimeStamp now = TimeStamp::Now();
  MOZ_RELEASE_ASSERT(!mContentfulPaint.IsNull(),
                     "TTI timeout with no contentful-paint?");

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  TimeStamp lastLongTaskEnded;
  mainThread->GetLastLongNonIdleTaskEnd(&lastLongTaskEnded);
  // Window starts at mContentfulPaint; any long task before that is ignored
  if (lastLongTaskEnded.IsNull() || lastLongTaskEnded < mContentfulPaint) {
    PAGELOAD_LOG(
        ("no longtask (last was %g ms before ContentfulPaint)",
         lastLongTaskEnded.IsNull()
             ? 0
             : (mContentfulPaint - lastLongTaskEnded).ToMilliseconds()));
    lastLongTaskEnded = mContentfulPaint;
  }
  TimeDuration delta = now - lastLongTaskEnded;
  PAGELOAD_LOG(("TTI delta: %g ms", delta.ToMilliseconds()));
  if (delta.ToMilliseconds() < TTI_WINDOW_SIZE_MS) {
    // Less than 5 seconds since the last long task or start of the window.
    // Schedule another check.
    PAGELOAD_LOG(("TTI: waiting additional %g ms",
                  (TTI_WINDOW_SIZE_MS + 100) - delta.ToMilliseconds()));
    aTimer->InitWithNamedFuncCallback(
        TTITimeoutCallback, this,
        (TTI_WINDOW_SIZE_MS + 100) -
            delta.ToMilliseconds(),  // slightly after the window ends
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
        "nsDOMNavigationTiming::TTITimeout");
    return;
  }

  // To correctly implement TTI/TTFI as proposed, we'd need to not
  // fire it until there are no more than 2 network loads.  By the
  // proposed definition, without that we're closer to
  // TimeToFirstInteractive.  There are also arguments about what sort
  // of loads should qualify.

  // XXX check number of network loads, and if > 2 mark to check if loads
  // decreases to 2 (or record that point and let the normal timer here
  // handle it)

  // TTI has occurred!  TTI is either FCP (if there are no longtasks and no
  // DCLEnd in the window that starts at FCP), or at the end of the last
  // Long Task or DOMContentLoadedEnd (whichever is later). lastLongTaskEnded
  // is >= FCP here.

  if (mTTFI.IsNull()) {
    // lastLongTaskEnded is >= mContentfulPaint
    mTTFI = (mDOMContentLoadedEventEnd.IsNull() ||
             lastLongTaskEnded > mDOMContentLoadedEventEnd)
                ? lastLongTaskEnded
                : mDOMContentLoadedEventEnd;
    PAGELOAD_LOG(
        ("TTFI after %dms (LongTask was at %dms, DCL was %dms)",
         int((mTTFI - mNavigationStart).ToMilliseconds()),
         lastLongTaskEnded.IsNull()
             ? 0
             : int((lastLongTaskEnded - mNavigationStart).ToMilliseconds()),
         mDOMContentLoadedEventEnd.IsNull()
             ? 0
             : int((mDOMContentLoadedEventEnd - mNavigationStart)
                       .ToMilliseconds())));
  }
  // XXX Implement TTI via check number of network loads, and if > 2 mark
  // to check if loads decreases to 2 (or record that point and let the
  // normal timer here handle it)

  mTTITimer = nullptr;

#ifdef MOZ_GECKO_PROFILER
  if (profiler_can_accept_markers() || PAGELOAD_LOG_ENABLED()) {
    TimeDuration elapsed = mTTFI - mNavigationStart;
    MOZ_ASSERT(elapsed.ToMilliseconds() > 0);
    TimeDuration elapsedLongTask =
        lastLongTaskEnded.IsNull() ? 0 : lastLongTaskEnded - mNavigationStart;
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker("TTFI after %dms (LongTask was at %dms) for URL %s",
                           int(elapsed.ToMilliseconds()),
                           int(elapsedLongTask.ToMilliseconds()), spec.get());

    PROFILER_ADD_MARKER_WITH_PAYLOAD(
        "TimeToFirstInteractive (TTFI)", DOM, TextMarkerPayload,
        (marker, mNavigationStart, mTTFI,
         profiler_get_inner_window_id_from_docshell(mDocShell)));
  }
#endif
}

void nsDOMNavigationTiming::NotifyNonBlankPaintForRootContentDocument() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mNavigationStart.IsNull());

  if (!mNonBlankPaint.IsNull()) {
    return;
  }

  mNonBlankPaint = TimeStamp::Now();

#ifdef MOZ_GECKO_PROFILER
  if (profiler_thread_is_being_profiled() || PAGELOAD_LOG_ENABLED()) {
    TimeDuration elapsed = mNonBlankPaint - mNavigationStart;
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker(
        "Non-blank paint after %dms for URL %s, %s",
        int(elapsed.ToMilliseconds()), spec.get(),
        mDocShellHasBeenActiveSinceNavigationStart
            ? "foreground tab"
            : "this tab was inactive some of the time between navigation start "
              "and first non-blank paint");
    PAGELOAD_LOG(("%s", marker.get()));
    PROFILER_ADD_MARKER_WITH_PAYLOAD(
        "FirstNonBlankPaint", DOM, TextMarkerPayload,
        (marker, mNavigationStart, mNonBlankPaint,
         profiler_get_inner_window_id_from_docshell(mDocShell)));
  }
#endif

  if (mDocShellHasBeenActiveSinceNavigationStart) {
    if (net::nsHttp::IsBeforeLastActiveTabLoadOptimization(mNavigationStart)) {
      Telemetry::AccumulateTimeDelta(
          Telemetry::TIME_TO_NON_BLANK_PAINT_NETOPT_MS, mNavigationStart,
          mNonBlankPaint);
    } else {
      Telemetry::AccumulateTimeDelta(
          Telemetry::TIME_TO_NON_BLANK_PAINT_NO_NETOPT_MS, mNavigationStart,
          mNonBlankPaint);
    }

    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_NON_BLANK_PAINT_MS,
                                   mNavigationStart, mNonBlankPaint);
  }
}

void nsDOMNavigationTiming::NotifyContentfulPaintForRootContentDocument(
    const mozilla::TimeStamp& aCompositeEndTime) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mNavigationStart.IsNull());

  if (!mContentfulPaint.IsNull()) {
    return;
  }

  mContentfulPaint = aCompositeEndTime;

#ifdef MOZ_GECKO_PROFILER
  if (profiler_can_accept_markers() || PAGELOAD_LOG_ENABLED()) {
    TimeDuration elapsed = mContentfulPaint - mNavigationStart;
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker(
        "Contentful paint after %dms for URL %s, %s",
        int(elapsed.ToMilliseconds()), spec.get(),
        mDocShellHasBeenActiveSinceNavigationStart
            ? "foreground tab"
            : "this tab was inactive some of the time between navigation start "
              "and first non-blank paint");
    PAGELOAD_LOG(("%s", marker.get()));
    PROFILER_ADD_MARKER_WITH_PAYLOAD(
        "FirstContentfulPaint", DOM, TextMarkerPayload,
        (marker, mNavigationStart, mContentfulPaint,
         profiler_get_inner_window_id_from_docshell(mDocShell)));
  }
#endif

  if (!mTTITimer) {
    mTTITimer = NS_NewTimer();
  }

  // TTI is first checked 5 seconds after the FCP (non-blank-paint is very close
  // to FCP).
  mTTITimer->InitWithNamedFuncCallback(TTITimeoutCallback, this,
                                       TTI_WINDOW_SIZE_MS,
                                       nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                       "nsDOMNavigationTiming::TTITimeout");

  if (mDocShellHasBeenActiveSinceNavigationStart) {
    Telemetry::AccumulateTimeDelta(Telemetry::TIME_TO_FIRST_CONTENTFUL_PAINT_MS,
                                   mNavigationStart, mContentfulPaint);
  }
}

void nsDOMNavigationTiming::NotifyDOMContentFlushedForRootContentDocument() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mNavigationStart.IsNull());

  if (!mDOMContentFlushed.IsNull()) {
    return;
  }

  mDOMContentFlushed = TimeStamp::Now();

#ifdef MOZ_GECKO_PROFILER
  if (profiler_thread_is_being_profiled() || PAGELOAD_LOG_ENABLED()) {
    TimeDuration elapsed = mDOMContentFlushed - mNavigationStart;
    nsAutoCString spec;
    if (mLoadedURI) {
      mLoadedURI->GetSpec(spec);
    }
    nsPrintfCString marker(
        "DOMContentFlushed after %dms for URL %s, %s",
        int(elapsed.ToMilliseconds()), spec.get(),
        mDocShellHasBeenActiveSinceNavigationStart
            ? "foreground tab"
            : "this tab was inactive some of the time between navigation start "
              "and DOMContentFlushed");
    PAGELOAD_LOG(("%s", marker.get()));
    PROFILER_ADD_MARKER_WITH_PAYLOAD(
        "DOMContentFlushed", DOM, TextMarkerPayload,
        (marker, mNavigationStart, mDOMContentFlushed,
         profiler_get_inner_window_id_from_docshell(mDocShell)));
  }
#endif
}

void nsDOMNavigationTiming::NotifyDocShellStateChanged(
    DocShellState aDocShellState) {
  mDocShellHasBeenActiveSinceNavigationStart &=
      (aDocShellState == DocShellState::eActive);
}

mozilla::TimeStamp nsDOMNavigationTiming::GetUnloadEventStartTimeStamp() const {
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  // todo: if you intend to update CheckSameOriginURI to log the error to the
  // console you also need to update the 'aFromPrivateWindow' argument.
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadStart;
  }
  return mozilla::TimeStamp();
}

mozilla::TimeStamp nsDOMNavigationTiming::GetUnloadEventEndTimeStamp() const {
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  // todo: if you intend to update CheckSameOriginURI to log the error to the
  // console you also need to update the 'aFromPrivateWindow' argument.
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadEnd;
  }
  return mozilla::TimeStamp();
}

bool nsDOMNavigationTiming::IsTopLevelContentDocumentInContentProcess() const {
  if (!mDocShell) {
    return false;
  }
  if (!XRE_IsContentProcess()) {
    return false;
  }
  return mDocShell->GetBrowsingContext()->IsTopContent();
}

nsDOMNavigationTiming::nsDOMNavigationTiming(nsDocShell* aDocShell,
                                             nsDOMNavigationTiming* aOther)
    : mDocShell(aDocShell),
      mUnloadedURI(aOther->mUnloadedURI),
      mLoadedURI(aOther->mLoadedURI),
      mNavigationType(aOther->mNavigationType),
      mNavigationStartHighRes(aOther->mNavigationStartHighRes),
      mNavigationStart(aOther->mNavigationStart),
      mNonBlankPaint(aOther->mNonBlankPaint),
      mContentfulPaint(aOther->mContentfulPaint),
      mDOMContentFlushed(aOther->mDOMContentFlushed),
      mBeforeUnloadStart(aOther->mBeforeUnloadStart),
      mUnloadStart(aOther->mUnloadStart),
      mUnloadEnd(aOther->mUnloadEnd),
      mLoadEventStart(aOther->mLoadEventStart),
      mLoadEventEnd(aOther->mLoadEventEnd),
      mDOMLoading(aOther->mDOMLoading),
      mDOMInteractive(aOther->mDOMInteractive),
      mDOMContentLoadedEventStart(aOther->mDOMContentLoadedEventStart),
      mDOMContentLoadedEventEnd(aOther->mDOMContentLoadedEventEnd),
      mDOMComplete(aOther->mDOMComplete),
      mTTFI(aOther->mTTFI),
      mDocShellHasBeenActiveSinceNavigationStart(
          aOther->mDocShellHasBeenActiveSinceNavigationStart) {}

/* static */
void mozilla::ipc::IPDLParamTraits<nsDOMNavigationTiming*>::Write(
    IPC::Message* aMsg, IProtocol* aActor, nsDOMNavigationTiming* aParam) {
  RefPtr<nsIURI> unloadedURI = aParam->mUnloadedURI.get();
  RefPtr<nsIURI> loadedURI = aParam->mLoadedURI.get();
  WriteIPDLParam(aMsg, aActor, unloadedURI ? Some(unloadedURI) : Nothing());
  WriteIPDLParam(aMsg, aActor, loadedURI ? Some(loadedURI) : Nothing());
  WriteIPDLParam(aMsg, aActor, uint32_t(aParam->mNavigationType));
  WriteIPDLParam(aMsg, aActor, aParam->mNavigationStartHighRes);
  WriteIPDLParam(aMsg, aActor, aParam->mNavigationStart);
  WriteIPDLParam(aMsg, aActor, aParam->mNonBlankPaint);
  WriteIPDLParam(aMsg, aActor, aParam->mContentfulPaint);
  WriteIPDLParam(aMsg, aActor, aParam->mDOMContentFlushed);
  WriteIPDLParam(aMsg, aActor, aParam->mBeforeUnloadStart);
  WriteIPDLParam(aMsg, aActor, aParam->mUnloadStart);
  WriteIPDLParam(aMsg, aActor, aParam->mUnloadEnd);
  WriteIPDLParam(aMsg, aActor, aParam->mLoadEventStart);
  WriteIPDLParam(aMsg, aActor, aParam->mLoadEventEnd);
  WriteIPDLParam(aMsg, aActor, aParam->mDOMLoading);
  WriteIPDLParam(aMsg, aActor, aParam->mDOMInteractive);
  WriteIPDLParam(aMsg, aActor, aParam->mDOMContentLoadedEventStart);
  WriteIPDLParam(aMsg, aActor, aParam->mDOMContentLoadedEventEnd);
  WriteIPDLParam(aMsg, aActor, aParam->mDOMComplete);
  WriteIPDLParam(aMsg, aActor, aParam->mTTFI);
  WriteIPDLParam(aMsg, aActor,
                 aParam->mDocShellHasBeenActiveSinceNavigationStart);
}

/* static */
bool mozilla::ipc::IPDLParamTraits<nsDOMNavigationTiming*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    RefPtr<nsDOMNavigationTiming>* aResult) {
  auto timing = MakeRefPtr<nsDOMNavigationTiming>(nullptr);
  uint32_t type;
  Maybe<RefPtr<nsIURI>> unloadedURI;
  Maybe<RefPtr<nsIURI>> loadedURI;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &unloadedURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &loadedURI) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &type) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mNavigationStartHighRes) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mNavigationStart) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mNonBlankPaint) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mContentfulPaint) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mDOMContentFlushed) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mBeforeUnloadStart) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mUnloadStart) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mUnloadEnd) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mLoadEventStart) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mLoadEventEnd) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mDOMLoading) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mDOMInteractive) ||
      !ReadIPDLParam(aMsg, aIter, aActor,
                     &timing->mDOMContentLoadedEventStart) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mDOMContentLoadedEventEnd) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mDOMComplete) ||
      !ReadIPDLParam(aMsg, aIter, aActor, &timing->mTTFI) ||
      !ReadIPDLParam(aMsg, aIter, aActor,
                     &timing->mDocShellHasBeenActiveSinceNavigationStart)) {
    return false;
  }
  timing->mNavigationType = nsDOMNavigationTiming::Type(type);
  if (unloadedURI) {
    timing->mUnloadedURI = std::move(*unloadedURI);
  }
  if (loadedURI) {
    timing->mLoadedURI = std::move(*loadedURI);
  }
  *aResult = std::move(timing);
  return true;
}
