/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "amIAddonManager.h"
#include "nsWindowMemoryReporter.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsIDOMWindowCollection.h"
#include "nsIEffectiveTLDService.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "XPCJSMemoryReporter.h"
#include "js/MemoryMetrics.h"
#include "nsQueryObject.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;

StaticRefPtr<nsWindowMemoryReporter> sWindowReporter;

/**
 * Don't trigger a ghost window check when a DOM window is detached if we've
 * run it this recently.
 */
const int32_t kTimeBetweenChecks = 45; /* seconds */

nsWindowMemoryReporter::nsWindowMemoryReporter()
  : mLastCheckForGhostWindows(TimeStamp::NowLoRes()),
    mCycleCollectorIsRunning(false),
    mCheckTimerWaitingForCCEnd(false),
    mGhostWindowCount(0)
{
}

nsWindowMemoryReporter::~nsWindowMemoryReporter()
{
  KillCheckTimer();
}

NS_IMPL_ISUPPORTS(nsWindowMemoryReporter, nsIMemoryReporter, nsIObserver,
                  nsISupportsWeakReference)

static nsresult
AddNonJSSizeOfWindowAndItsDescendents(nsGlobalWindow* aWindow,
                                      nsTabSizes* aSizes)
{
  // Measure the window.
  nsWindowSizes windowSizes(moz_malloc_size_of);
  aWindow->AddSizeOfIncludingThis(&windowSizes);
  windowSizes.addToTabSizes(aSizes);

  // Measure the inner window, if there is one.
  nsWindowSizes innerWindowSizes(moz_malloc_size_of);
  nsGlobalWindow* inner = aWindow->IsOuterWindow() ? aWindow->GetCurrentInnerWindowInternal()
                                                   : nullptr;
  if (inner) {
    inner->AddSizeOfIncludingThis(&innerWindowSizes);
    innerWindowSizes.addToTabSizes(aSizes);
  }

  nsCOMPtr<nsIDOMWindowCollection> frames = aWindow->GetFrames();

  uint32_t length;
  nsresult rv = frames->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  // Measure this window's descendents.
  for (uint32_t i = 0; i < length; i++) {
      nsCOMPtr<mozIDOMWindowProxy> child;
      rv = frames->Item(i, getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_STATE(child);

      nsGlobalWindow* childWin = nsGlobalWindow::Cast(child);

      rv = AddNonJSSizeOfWindowAndItsDescendents(childWin, aSizes);
      NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

static nsresult
NonJSSizeOfTab(nsPIDOMWindowOuter* aWindow, size_t* aDomSize, size_t* aStyleSize, size_t* aOtherSize)
{
  nsGlobalWindow* window = nsGlobalWindow::Cast(aWindow);

  nsTabSizes sizes;
  nsresult rv = AddNonJSSizeOfWindowAndItsDescendents(window, &sizes);
  NS_ENSURE_SUCCESS(rv, rv);

  *aDomSize   = sizes.mDom;
  *aStyleSize = sizes.mStyle;
  *aOtherSize = sizes.mOther;
  return NS_OK;
}

/* static */ void
nsWindowMemoryReporter::Init()
{
  MOZ_ASSERT(!sWindowReporter);
  sWindowReporter = new nsWindowMemoryReporter();
  ClearOnShutdown(&sWindowReporter);
  RegisterStrongMemoryReporter(sWindowReporter);
  RegisterNonJSSizeOfTab(NonJSSizeOfTab);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(sWindowReporter, "after-minimize-memory-usage",
                    /* weakRef = */ true);
    os->AddObserver(sWindowReporter, "cycle-collector-begin",
                    /* weakRef = */ true);
    os->AddObserver(sWindowReporter, "cycle-collector-end",
                    /* weakRef = */ true);
  }

  RegisterGhostWindowsDistinguishedAmount(GhostWindowsDistinguishedAmount);
}

/* static */ nsWindowMemoryReporter*
nsWindowMemoryReporter::Get()
{
  return sWindowReporter;
}

static already_AddRefed<nsIURI>
GetWindowURI(nsGlobalWindow* aWindow)
{
  NS_ENSURE_TRUE(aWindow, nullptr);

  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  nsCOMPtr<nsIURI> uri;

  if (doc) {
    uri = doc->GetDocumentURI();
  }

  if (!uri) {
    nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrincipal =
      do_QueryObject(aWindow);
    NS_ENSURE_TRUE(scriptObjPrincipal, nullptr);

    // GetPrincipal() will print a warning if the window does not have an outer
    // window, so check here for an outer window first.  This code is
    // functionally correct if we leave out the GetOuterWindow() check, but we
    // end up printing a lot of warnings during debug mochitests.
    if (aWindow->GetOuterWindow()) {
      nsIPrincipal* principal = scriptObjPrincipal->GetPrincipal();
      if (principal) {
        principal->GetURI(getter_AddRefs(uri));
      }
    }
  }

  return uri.forget();
}

static void
AppendWindowURI(nsGlobalWindow *aWindow, nsACString& aStr, bool aAnonymize)
{
  nsCOMPtr<nsIURI> uri = GetWindowURI(aWindow);

  if (uri) {
    if (aAnonymize && !aWindow->IsChromeWindow()) {
      aStr.AppendPrintf("<anonymized-%" PRIu64 ">", aWindow->WindowID());
    } else {
      nsCString spec = uri->GetSpecOrDefault();

      // A hack: replace forward slashes with '\\' so they aren't
      // treated as path separators.  Users of the reporters
      // (such as about:memory) have to undo this change.
      spec.ReplaceChar('/', '\\');

      aStr += spec;
    }
  } else {
    // If we're unable to find a URI, we're dealing with a chrome window with
    // no document in it (or somesuch), so we call this a "system window".
    aStr += NS_LITERAL_CSTRING("[system]");
  }
}

MOZ_DEFINE_MALLOC_SIZE_OF(WindowsMallocSizeOf)

// The key is the window ID.
typedef nsDataHashtable<nsUint64HashKey, nsCString> WindowPaths;

static void
ReportAmount(const nsCString& aBasePath, const char* aPathTail,
             size_t aAmount, const nsCString& aDescription,
             uint32_t aKind, uint32_t aUnits,
             nsIHandleReportCallback* aHandleReport,
             nsISupports* aData)
{
  if (aAmount == 0) {
    return;
  }

  nsAutoCString path(aBasePath);
  path += aPathTail;

  aHandleReport->Callback(
    EmptyCString(), path, aKind, aUnits, aAmount, aDescription, aData);
}

static void
ReportSize(const nsCString& aBasePath, const char* aPathTail,
           size_t aAmount, const nsCString& aDescription,
           nsIHandleReportCallback* aHandleReport,
           nsISupports* aData)
{
  ReportAmount(aBasePath, aPathTail, aAmount, aDescription,
               nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
               aHandleReport, aData);
}

static void
ReportCount(const nsCString& aBasePath, const char* aPathTail,
            size_t aAmount, const nsCString& aDescription,
            nsIHandleReportCallback* aHandleReport,
            nsISupports* aData)
{
  ReportAmount(aBasePath, aPathTail, aAmount, aDescription,
               nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
               aHandleReport, aData);
}

static void
CollectWindowReports(nsGlobalWindow *aWindow,
                     amIAddonManager *addonManager,
                     nsWindowSizes *aWindowTotalSizes,
                     nsTHashtable<nsUint64HashKey> *aGhostWindowIDs,
                     WindowPaths *aWindowPaths,
                     WindowPaths *aTopWindowPaths,
                     nsIHandleReportCallback *aHandleReport,
                     nsISupports *aData,
                     bool aAnonymize)
{
  nsAutoCString windowPath("explicit/");

  // Avoid calling aWindow->GetTop() if there's no outer window.  It will work
  // just fine, but will spew a lot of warnings.
  nsGlobalWindow *top = nullptr;
  nsCOMPtr<nsIURI> location;
  if (aWindow->GetOuterWindow()) {
    // Our window should have a null top iff it has a null docshell.
    MOZ_ASSERT(!!aWindow->GetTopInternal() == !!aWindow->GetDocShell());
    top = aWindow->GetTopInternal();
    if (top) {
      location = GetWindowURI(top);
    }
  }
  if (!location) {
    location = GetWindowURI(aWindow);
  }

  if (addonManager && location) {
    bool ok;
    nsAutoCString id;
    if (NS_SUCCEEDED(addonManager->MapURIToAddonID(location, id, &ok)) && ok) {
      // Add-on names are not privacy-sensitive, so we can use them with
      // impunity.
      windowPath += NS_LITERAL_CSTRING("add-ons/") + id +
                    NS_LITERAL_CSTRING("/");
    }
  }

  windowPath += NS_LITERAL_CSTRING("window-objects/");

  if (top) {
    windowPath += NS_LITERAL_CSTRING("top(");
    AppendWindowURI(top, windowPath, aAnonymize);
    windowPath.AppendPrintf(", id=%" PRIu64 ")", top->WindowID());

    aTopWindowPaths->Put(aWindow->WindowID(), windowPath);

    windowPath += aWindow->IsFrozen() ? NS_LITERAL_CSTRING("/cached/")
                                      : NS_LITERAL_CSTRING("/active/");
  } else {
    if (aGhostWindowIDs->Contains(aWindow->WindowID())) {
      windowPath += NS_LITERAL_CSTRING("top(none)/ghost/");
    } else {
      windowPath += NS_LITERAL_CSTRING("top(none)/detached/");
    }
  }

  windowPath += NS_LITERAL_CSTRING("window(");
  AppendWindowURI(aWindow, windowPath, aAnonymize);
  windowPath += NS_LITERAL_CSTRING(")");

  // Use |windowPath|, but replace "explicit/" with "event-counts/".
  nsCString censusWindowPath(windowPath);
  censusWindowPath.Replace(0, strlen("explicit"), "event-counts");

  // Remember the path for later.
  aWindowPaths->Put(aWindow->WindowID(), windowPath);

#define REPORT_SIZE(_pathTail, _amount, _desc) \
  ReportSize(windowPath, _pathTail, _amount, NS_LITERAL_CSTRING(_desc), \
             aHandleReport, aData);

#define REPORT_COUNT(_pathTail, _amount, _desc) \
  ReportCount(censusWindowPath, _pathTail, _amount, NS_LITERAL_CSTRING(_desc), \
              aHandleReport, aData);

  nsWindowSizes windowSizes(WindowsMallocSizeOf);
  aWindow->AddSizeOfIncludingThis(&windowSizes);

  REPORT_SIZE("/dom/element-nodes", windowSizes.mDOMElementNodesSize,
              "Memory used by the element nodes in a window's DOM.");
  aWindowTotalSizes->mDOMElementNodesSize += windowSizes.mDOMElementNodesSize;

  REPORT_SIZE("/dom/text-nodes", windowSizes.mDOMTextNodesSize,
              "Memory used by the text nodes in a window's DOM.");
  aWindowTotalSizes->mDOMTextNodesSize += windowSizes.mDOMTextNodesSize;

  REPORT_SIZE("/dom/cdata-nodes", windowSizes.mDOMCDATANodesSize,
              "Memory used by the CDATA nodes in a window's DOM.");
  aWindowTotalSizes->mDOMCDATANodesSize += windowSizes.mDOMCDATANodesSize;

  REPORT_SIZE("/dom/comment-nodes", windowSizes.mDOMCommentNodesSize,
              "Memory used by the comment nodes in a window's DOM.");
  aWindowTotalSizes->mDOMCommentNodesSize += windowSizes.mDOMCommentNodesSize;

  REPORT_SIZE("/dom/event-targets", windowSizes.mDOMEventTargetsSize,
              "Memory used by the event targets table in a window's DOM, and "
              "the objects it points to, which include XHRs.");
  aWindowTotalSizes->mDOMEventTargetsSize += windowSizes.mDOMEventTargetsSize;

  REPORT_COUNT("/dom/event-targets", windowSizes.mDOMEventTargetsCount,
               "Number of non-node event targets in the event targets table "
               "in a window's DOM, such as XHRs.");
  aWindowTotalSizes->mDOMEventTargetsCount +=
    windowSizes.mDOMEventTargetsCount;

  REPORT_COUNT("/dom/event-listeners", windowSizes.mDOMEventListenersCount,
               "Number of event listeners in a window, including event "
               "listeners on nodes and other event targets.");
  aWindowTotalSizes->mDOMEventListenersCount +=
    windowSizes.mDOMEventListenersCount;

  REPORT_SIZE("/dom/other", windowSizes.mDOMOtherSize,
              "Memory used by a window's DOM that isn't measured by the "
              "other 'dom/' numbers.");
  aWindowTotalSizes->mDOMOtherSize += windowSizes.mDOMOtherSize;

  REPORT_SIZE("/property-tables",
              windowSizes.mPropertyTablesSize,
              "Memory used for the property tables within a window.");
  aWindowTotalSizes->mPropertyTablesSize += windowSizes.mPropertyTablesSize;

  REPORT_SIZE("/style-sheets", windowSizes.mStyleSheetsSize,
              "Memory used by style sheets within a window.");
  aWindowTotalSizes->mStyleSheetsSize += windowSizes.mStyleSheetsSize;

  REPORT_SIZE("/layout/pres-shell", windowSizes.mLayoutPresShellSize,
              "Memory used by layout's PresShell, along with any structures "
              "allocated in its arena and not measured elsewhere, "
              "within a window.");
  aWindowTotalSizes->mLayoutPresShellSize += windowSizes.mLayoutPresShellSize;

  REPORT_SIZE("/layout/line-boxes", windowSizes.mArenaStats.mLineBoxes,
              "Memory used by line boxes within a window.");
  aWindowTotalSizes->mArenaStats.mLineBoxes
    += windowSizes.mArenaStats.mLineBoxes;

  REPORT_SIZE("/layout/rule-nodes", windowSizes.mArenaStats.mRuleNodes,
              "Memory used by CSS rule nodes within a window.");
  aWindowTotalSizes->mArenaStats.mRuleNodes
    += windowSizes.mArenaStats.mRuleNodes;

  REPORT_SIZE("/layout/style-contexts", windowSizes.mArenaStats.mStyleContexts,
              "Memory used by style contexts within a window.");
  aWindowTotalSizes->mArenaStats.mStyleContexts
    += windowSizes.mArenaStats.mStyleContexts;

  REPORT_SIZE("/layout/style-structs", windowSizes.mArenaStats.mStyleStructs,
              "Memory used by style structs within a window.");
  aWindowTotalSizes->mArenaStats.mStyleStructs
    += windowSizes.mArenaStats.mStyleStructs;

  REPORT_SIZE("/layout/style-sets", windowSizes.mLayoutStyleSetsSize,
              "Memory used by style sets within a window.");
  aWindowTotalSizes->mLayoutStyleSetsSize += windowSizes.mLayoutStyleSetsSize;

  REPORT_SIZE("/layout/text-runs", windowSizes.mLayoutTextRunsSize,
              "Memory used for text-runs (glyph layout) in the PresShell's "
              "frame tree, within a window.");
  aWindowTotalSizes->mLayoutTextRunsSize += windowSizes.mLayoutTextRunsSize;

  REPORT_SIZE("/layout/pres-contexts", windowSizes.mLayoutPresContextSize,
         "Memory used for the PresContext in the PresShell's frame "
         "within a window.");
  aWindowTotalSizes->mLayoutPresContextSize +=
    windowSizes.mLayoutPresContextSize;

  REPORT_SIZE("/layout/frame-properties", windowSizes.mLayoutFramePropertiesSize,
         "Memory used for frame properties attached to frames "
         "within a window.");
  aWindowTotalSizes->mLayoutFramePropertiesSize +=
    windowSizes.mLayoutFramePropertiesSize;

  REPORT_SIZE("/dom/performance/user-entries",
              windowSizes.mDOMPerformanceUserEntries,
              "Memory used for performance user entries.");
  aWindowTotalSizes->mDOMPerformanceUserEntries +=
    windowSizes.mDOMPerformanceUserEntries;

  REPORT_SIZE("/dom/performance/resource-entries",
              windowSizes.mDOMPerformanceResourceEntries,
              "Memory used for performance resource entries.");
  aWindowTotalSizes->mDOMPerformanceResourceEntries +=
    windowSizes.mDOMPerformanceResourceEntries;

  // There are many different kinds of frames, but it is very likely
  // that only a few matter.  Implement a cutoff so we don't bloat
  // about:memory with many uninteresting entries.
  const size_t FRAME_SUNDRIES_THRESHOLD =
    js::MemoryReportingSundriesThreshold();

  size_t frameSundriesSize = 0;
#define FRAME_ID(classname, ...)                                        \
  {                                                                     \
    size_t frameSize                                                    \
      = windowSizes.mArenaStats.FRAME_ID_STAT_FIELD(classname);         \
    if (frameSize < FRAME_SUNDRIES_THRESHOLD) {                         \
      frameSundriesSize += frameSize;                                   \
    } else {                                                            \
      REPORT_SIZE("/layout/frames/" # classname, frameSize,             \
                  "Memory used by frames of "                           \
                  "type " #classname " within a window.");              \
    }                                                                   \
    aWindowTotalSizes->mArenaStats.FRAME_ID_STAT_FIELD(classname)       \
      += frameSize;                                                     \
  }
#define ABSTRACT_FRAME_ID(...)
#include "nsFrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID

  if (frameSundriesSize > 0) {
    REPORT_SIZE("/layout/frames/sundries", frameSundriesSize,
                "The sum of all memory used by frames which were too small "
                "to be shown individually.");
  }

#undef REPORT_SIZE
#undef REPORT_COUNT
}

typedef nsTArray< RefPtr<nsGlobalWindow> > WindowArray;

NS_IMETHODIMP
nsWindowMemoryReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                       nsISupports* aData, bool aAnonymize)
{
  nsGlobalWindow::WindowByIdTable* windowsById =
    nsGlobalWindow::GetWindowsTable();
  NS_ENSURE_TRUE(windowsById, NS_OK);

  // Hold on to every window in memory so that window objects can't be
  // destroyed while we're calling the memory reporter callback.
  WindowArray windows;
  for (auto iter = windowsById->Iter(); !iter.Done(); iter.Next()) {
    windows.AppendElement(iter.Data());
  }

  // Get the IDs of all the "ghost" windows, and call aHandleReport->Callback()
  // for each one.
  nsTHashtable<nsUint64HashKey> ghostWindows;
  CheckForGhostWindows(&ghostWindows);
  for (auto iter = ghostWindows.ConstIter(); !iter.Done(); iter.Next()) {
    nsGlobalWindow::WindowByIdTable* windowsById =
      nsGlobalWindow::GetWindowsTable();
    if (!windowsById) {
      NS_WARNING("Couldn't get window-by-id hashtable?");
      continue;
    }

    nsGlobalWindow* window = windowsById->Get(iter.Get()->GetKey());
    if (!window) {
      NS_WARNING("Could not look up window?");
      continue;
    }

    nsAutoCString path;
    path.AppendLiteral("ghost-windows/");
    AppendWindowURI(window, path, aAnonymize);

    aHandleReport->Callback(
      /* process = */ EmptyCString(),
      path,
      nsIMemoryReporter::KIND_OTHER,
      nsIMemoryReporter::UNITS_COUNT,
      /* amount = */ 1,
      /* description = */ NS_LITERAL_CSTRING("A ghost window."),
      aData);
  }

  MOZ_COLLECT_REPORT(
    "ghost-windows", KIND_OTHER, UNITS_COUNT, ghostWindows.Count(),
"The number of ghost windows present (the number of nodes underneath "
"explicit/window-objects/top(none)/ghost, modulo race conditions).  A ghost "
"window is not shown in any tab, does not share a domain with any non-detached "
"windows, and has met these criteria for at least "
"memory.ghost_window_timeout_seconds, or has survived a round of "
"about:memory's minimize memory usage button.\n\n"
"Ghost windows can happen legitimately, but they are often indicative of "
"leaks in the browser or add-ons.");

  WindowPaths windowPaths;
  WindowPaths topWindowPaths;

  // Collect window memory usage.
  nsWindowSizes windowTotalSizes(nullptr);
  nsCOMPtr<amIAddonManager> addonManager;
  if (XRE_IsParentProcess()) {
    // Only try to access the service from the main process.
    addonManager = do_GetService("@mozilla.org/addons/integration;1");
  }
  for (uint32_t i = 0; i < windows.Length(); i++) {
    CollectWindowReports(windows[i], addonManager,
                         &windowTotalSizes, &ghostWindows,
                         &windowPaths, &topWindowPaths, aHandleReport,
                         aData, aAnonymize);
  }

  // Report JS memory usage.  We do this from here because the JS memory
  // reporter needs to be passed |windowPaths|.
  xpc::JSReporter::CollectReports(&windowPaths, &topWindowPaths,
                                  aHandleReport, aData, aAnonymize);

#define REPORT(_path, _amount, _desc) \
  aHandleReport->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path), \
                          KIND_OTHER, UNITS_BYTES, _amount, \
                          NS_LITERAL_CSTRING(_desc), aData);

  REPORT("window-objects/dom/element-nodes", windowTotalSizes.mDOMElementNodesSize,
         "This is the sum of all windows' 'dom/element-nodes' numbers.");

  REPORT("window-objects/dom/text-nodes", windowTotalSizes.mDOMTextNodesSize,
         "This is the sum of all windows' 'dom/text-nodes' numbers.");

  REPORT("window-objects/dom/cdata-nodes", windowTotalSizes.mDOMCDATANodesSize,
         "This is the sum of all windows' 'dom/cdata-nodes' numbers.");

  REPORT("window-objects/dom/comment-nodes", windowTotalSizes.mDOMCommentNodesSize,
         "This is the sum of all windows' 'dom/comment-nodes' numbers.");

  REPORT("window-objects/dom/event-targets", windowTotalSizes.mDOMEventTargetsSize,
         "This is the sum of all windows' 'dom/event-targets' numbers.");

  REPORT("window-objects/dom/other", windowTotalSizes.mDOMOtherSize,
         "This is the sum of all windows' 'dom/other' numbers.");

  REPORT("window-objects/property-tables",
         windowTotalSizes.mPropertyTablesSize,
         "This is the sum of all windows' 'property-tables' numbers.");

  REPORT("window-objects/style-sheets", windowTotalSizes.mStyleSheetsSize,
         "This is the sum of all windows' 'style-sheets' numbers.");

  REPORT("window-objects/layout/pres-shell", windowTotalSizes.mLayoutPresShellSize,
         "This is the sum of all windows' 'layout/arenas' numbers.");

  REPORT("window-objects/layout/line-boxes",
         windowTotalSizes.mArenaStats.mLineBoxes,
         "This is the sum of all windows' 'layout/line-boxes' numbers.");

  REPORT("window-objects/layout/rule-nodes",
         windowTotalSizes.mArenaStats.mRuleNodes,
         "This is the sum of all windows' 'layout/rule-nodes' numbers.");

  REPORT("window-objects/layout/style-contexts",
         windowTotalSizes.mArenaStats.mStyleContexts,
         "This is the sum of all windows' 'layout/style-contexts' numbers.");

  REPORT("window-objects/layout/style-structs",
         windowTotalSizes.mArenaStats.mStyleStructs,
         "This is the sum of all windows' 'layout/style-structs' numbers.");

  REPORT("window-objects/layout/style-sets", windowTotalSizes.mLayoutStyleSetsSize,
         "This is the sum of all windows' 'layout/style-sets' numbers.");

  REPORT("window-objects/layout/text-runs", windowTotalSizes.mLayoutTextRunsSize,
         "This is the sum of all windows' 'layout/text-runs' numbers.");

  REPORT("window-objects/layout/pres-contexts", windowTotalSizes.mLayoutPresContextSize,
         "This is the sum of all windows' 'layout/pres-contexts' numbers.");

  REPORT("window-objects/layout/frame-properties", windowTotalSizes.mLayoutFramePropertiesSize,
         "This is the sum of all windows' 'layout/frame-properties' numbers.");

  size_t frameTotal = 0;
#define FRAME_ID(classname, ...)                \
  frameTotal += windowTotalSizes.mArenaStats.FRAME_ID_STAT_FIELD(classname);
#define ABSTRACT_FRAME_ID(...)
#include "nsFrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID

  REPORT("window-objects/layout/frames", frameTotal,
         "Memory used for layout frames within windows. "
         "This is the sum of all windows' 'layout/frames/' numbers.");

#undef REPORT

  return NS_OK;
}

uint32_t
nsWindowMemoryReporter::GetGhostTimeout()
{
  return Preferences::GetUint("memory.ghost_window_timeout_seconds", 60);
}

NS_IMETHODIMP
nsWindowMemoryReporter::Observe(nsISupports *aSubject, const char *aTopic,
                                const char16_t *aData)
{
  if (!strcmp(aTopic, "after-minimize-memory-usage")) {
    ObserveAfterMinimizeMemoryUsage();
  } else if (!strcmp(aTopic, "cycle-collector-begin")) {
    if (mCheckTimer) {
      mCheckTimerWaitingForCCEnd = true;
      KillCheckTimer();
    }
    mCycleCollectorIsRunning = true;
  } else if (!strcmp(aTopic, "cycle-collector-end")) {
    mCycleCollectorIsRunning = false;
    if (mCheckTimerWaitingForCCEnd) {
      mCheckTimerWaitingForCCEnd = false;
      AsyncCheckForGhostWindows();
    }
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

void
nsWindowMemoryReporter::ObserveDOMWindowDetached(nsGlobalWindow* aWindow)
{
  nsWeakPtr weakWindow = do_GetWeakReference(static_cast<nsIDOMEventTarget*>(aWindow));
  if (!weakWindow) {
    NS_WARNING("Couldn't take weak reference to a window?");
    return;
  }

  mDetachedWindows.Put(weakWindow, TimeStamp());

  AsyncCheckForGhostWindows();
}

// static
void
nsWindowMemoryReporter::CheckTimerFired(nsITimer* aTimer, void* aData)
{
  if (sWindowReporter) {
    MOZ_ASSERT(!sWindowReporter->mCycleCollectorIsRunning);
    sWindowReporter->CheckForGhostWindows();
  }
}

void
nsWindowMemoryReporter::AsyncCheckForGhostWindows()
{
  if (mCheckTimer) {
    return;
  }

  if (mCycleCollectorIsRunning) {
    mCheckTimerWaitingForCCEnd = true;
    return;
  }

  // If more than kTimeBetweenChecks seconds have elapsed since the last check,
  // timerDelay is 0.  Otherwise, it is kTimeBetweenChecks, reduced by the time
  // since the last check.  Reducing the delay by the time since the last check
  // prevents the timer from being completely starved if it is repeatedly killed
  // and restarted.
  int32_t timeSinceLastCheck = (TimeStamp::NowLoRes() - mLastCheckForGhostWindows).ToSeconds();
  int32_t timerDelay = (kTimeBetweenChecks - std::min(timeSinceLastCheck, kTimeBetweenChecks)) * PR_MSEC_PER_SEC;

  mCheckTimer = do_CreateInstance("@mozilla.org/timer;1");

  if (mCheckTimer) {
    mCheckTimer->InitWithNamedFuncCallback(CheckTimerFired, nullptr,
                                           timerDelay, nsITimer::TYPE_ONE_SHOT,
                                           "nsWindowMemoryReporter::AsyncCheckForGhostWindows_timer");
  }
}

void
nsWindowMemoryReporter::ObserveAfterMinimizeMemoryUsage()
{
  // Someone claims they've done enough GC/CCs so that all eligible windows
  // have been free'd.  So we deem that any windows which satisfy ghost
  // criteria (1) and (2) now satisfy criterion (3) as well.
  //
  // To effect this change, we'll backdate some of our timestamps.

  TimeStamp minTimeStamp = TimeStamp::Now() -
                           TimeDuration::FromSeconds(GetGhostTimeout());

  for (auto iter = mDetachedWindows.Iter(); !iter.Done(); iter.Next()) {
    TimeStamp& timeStamp = iter.Data();
    if (!timeStamp.IsNull() && timeStamp > minTimeStamp) {
      timeStamp = minTimeStamp;
    }
  }
}

/**
 * Iterate over mDetachedWindows and update it to reflect the current state of
 * the world.  In particular:
 *
 *   - Remove weak refs to windows which no longer exist.
 *
 *   - Remove references to windows which are no longer detached.
 *
 *   - Reset the timestamp on detached windows which share a domain with a
 *     non-detached window (they no longer meet ghost criterion (2)).
 *
 *   - If a window now meets ghost criterion (2) but didn't before, set its
 *     timestamp to now.
 *
 * Additionally, if aOutGhostIDs is not null, fill it with the window IDs of
 * all ghost windows we found.
 */
void
nsWindowMemoryReporter::CheckForGhostWindows(
  nsTHashtable<nsUint64HashKey> *aOutGhostIDs /* = nullptr */)
{
  nsCOMPtr<nsIEffectiveTLDService> tldService = do_GetService(
    NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    NS_WARNING("Couldn't get TLDService.");
    return;
  }

  nsGlobalWindow::WindowByIdTable *windowsById =
    nsGlobalWindow::GetWindowsTable();
  if (!windowsById) {
    NS_WARNING("GetWindowsTable returned null");
    return;
  }

  mLastCheckForGhostWindows = TimeStamp::NowLoRes();
  KillCheckTimer();

  nsTHashtable<nsCStringHashKey> nonDetachedWindowDomains;
  nsDataHashtable<nsISupportsHashKey, nsCString> domainMap;

  // Populate nonDetachedWindowDomains.
  for (auto iter = windowsById->Iter(); !iter.Done(); iter.Next()) {
    // Null outer window implies null top, but calling GetTop() when there's no
    // outer window causes us to spew debug warnings.
    nsGlobalWindow* window = iter.UserData();
    if (!window->GetOuterWindow() || !window->GetTopInternal()) {
      // This window is detached, so we don't care about its domain.
      continue;
    }

    nsCOMPtr<nsIURI> uri = GetWindowURI(window);
    nsAutoCString domain;
    if (uri) {
      domain = domainMap.LookupForAdd(uri).OrInsert([&]() {
        nsCString d;
        tldService->GetBaseDomain(uri, 0, d);
        return d;
      });
    }

    nonDetachedWindowDomains.PutEntry(domain);
  }

  // Update mDetachedWindows and write the ghost window IDs into aOutGhostIDs,
  // if it's not null.
  uint32_t ghostTimeout = GetGhostTimeout();
  TimeStamp now = mLastCheckForGhostWindows;
  mGhostWindowCount = 0;
  for (auto iter = mDetachedWindows.Iter(); !iter.Done(); iter.Next()) {
    nsWeakPtr weakKey = do_QueryInterface(iter.Key());
    nsCOMPtr<mozIDOMWindow> iwindow = do_QueryReferent(weakKey);
    if (!iwindow) {
      // The window object has been destroyed.  Stop tracking its weak ref in
      // our hashtable.
      iter.Remove();
      continue;
    }

    nsPIDOMWindowInner* window = nsPIDOMWindowInner::From(iwindow);

    // Avoid calling GetTop() if we have no outer window.  Nothing will break if
    // we do, but it will spew debug output, which can cause our test logs to
    // overflow.
    nsCOMPtr<nsPIDOMWindowOuter> top;
    if (window->GetOuterWindow()) {
      top = window->GetOuterWindow()->GetTop();
    }

    if (top) {
      // The window is no longer detached, so we no longer want to track it.
      iter.Remove();
      continue;
    }

    nsCOMPtr<nsIURI> uri = GetWindowURI(nsGlobalWindow::Cast(window));

    nsAutoCString domain;
    if (uri) {
      // GetBaseDomain works fine if |uri| is null, but it outputs a warning
      // which ends up overrunning the mochitest logs.
      tldService->GetBaseDomain(uri, 0, domain);
    }

    TimeStamp& timeStamp = iter.Data();

    if (nonDetachedWindowDomains.Contains(domain)) {
      // This window shares a domain with a non-detached window, so reset its
      // clock.
      timeStamp = TimeStamp();
    } else {
      // This window does not share a domain with a non-detached window, so it
      // meets ghost criterion (2).
      if (timeStamp.IsNull()) {
        // This may become a ghost window later; start its clock.
        timeStamp = now;
      } else if ((now - timeStamp).ToSeconds() > ghostTimeout) {
        // This definitely is a ghost window, so add it to aOutGhostIDs, if
        // that is not null.
        mGhostWindowCount++;
        if (aOutGhostIDs && window) {
          aOutGhostIDs->PutEntry(window->WindowID());
        }
      }
    }
  }
}

/* static */ int64_t
nsWindowMemoryReporter::GhostWindowsDistinguishedAmount()
{
  return sWindowReporter->mGhostWindowCount;
}

void
nsWindowMemoryReporter::KillCheckTimer()
{
  if (mCheckTimer) {
    mCheckTimer->Cancel();
    mCheckTimer = nullptr;
  }
}

#ifdef DEBUG
/* static */ void
nsWindowMemoryReporter::UnlinkGhostWindows()
{
  if (!sWindowReporter) {
    return;
  }

  nsGlobalWindow::WindowByIdTable* windowsById =
    nsGlobalWindow::GetWindowsTable();
  if (!windowsById) {
    return;
  }

  // Hold on to every window in memory so that window objects can't be
  // destroyed while we're calling the UnlinkGhostWindows callback.
  WindowArray windows;
  for (auto iter = windowsById->Iter(); !iter.Done(); iter.Next()) {
    windows.AppendElement(iter.Data());
  }

  // Get the IDs of all the "ghost" windows, and unlink them all.
  nsTHashtable<nsUint64HashKey> ghostWindows;
  sWindowReporter->CheckForGhostWindows(&ghostWindows);
  for (auto iter = ghostWindows.ConstIter(); !iter.Done(); iter.Next()) {
    nsGlobalWindow::WindowByIdTable* windowsById =
      nsGlobalWindow::GetWindowsTable();
    if (!windowsById) {
      continue;
    }

    RefPtr<nsGlobalWindow> window = windowsById->Get(iter.Get()->GetKey());
    if (window) {
      window->RiskyUnlink();
    }
  }
}
#endif
