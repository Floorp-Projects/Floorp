/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWindowMemoryReporter.h"
#include "nsGlobalWindow.h"
#include "nsIEffectiveTLDService.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"

using namespace mozilla;

nsWindowMemoryReporter::nsWindowMemoryReporter()
  : mCheckForGhostWindowsCallbackPending(false)
{
  mDetachedWindows.Init();
}

NS_IMPL_ISUPPORTS3(nsWindowMemoryReporter, nsIMemoryMultiReporter, nsIObserver,
                   nsSupportsWeakReference)

/* static */
void
nsWindowMemoryReporter::Init()
{
  // The memory reporter manager will own this object.
  nsWindowMemoryReporter *windowReporter = new nsWindowMemoryReporter();
  NS_RegisterMemoryMultiReporter(windowReporter);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    // DOM_WINDOW_DESTROYED_TOPIC announces what we call window "detachment",
    // when a window's docshell is set to NULL.
    os->AddObserver(windowReporter, DOM_WINDOW_DESTROYED_TOPIC,
                    /* weakRef = */ true);
    os->AddObserver(windowReporter, "after-minimize-memory-usage",
                    /* weakRef = */ true);
  }

  GhostURLsReporter *ghostMultiReporter =
    new GhostURLsReporter(windowReporter);
  NS_RegisterMemoryMultiReporter(ghostMultiReporter);

  NumGhostsReporter *ghostReporter =
    new NumGhostsReporter(windowReporter);
  NS_RegisterMemoryReporter(ghostReporter);
}

static already_AddRefed<nsIURI>
GetWindowURI(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(pWindow, NULL);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(pWindow->GetExtantDocument());
  nsCOMPtr<nsIURI> uri;

  if (doc) {
    uri = doc->GetDocumentURI();
  }

  if (!uri) {
    nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrincipal =
      do_QueryInterface(aWindow);
    NS_ENSURE_TRUE(scriptObjPrincipal, NULL);

    nsIPrincipal *principal = scriptObjPrincipal->GetPrincipal();

    if (principal) {
      principal->GetURI(getter_AddRefs(uri));
    }
  }

  return uri.forget();
}

static void
AppendWindowURI(nsGlobalWindow *aWindow, nsACString& aStr)
{
  nsCOMPtr<nsIURI> uri = GetWindowURI(aWindow);

  if (uri) {
    nsCString spec;
    uri->GetSpec(spec);

    // A hack: replace forward slashes with '\\' so they aren't
    // treated as path separators.  Users of the reporters
    // (such as about:memory) have to undo this change.
    spec.ReplaceChar('/', '\\');

    aStr += spec;
  } else {
    // If we're unable to find a URI, we're dealing with a chrome window with
    // no document in it (or somesuch), so we call this a "system window".
    aStr += NS_LITERAL_CSTRING("[system]");
  }
}

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(DOMStyleMallocSizeOf, "windows")

static nsresult
CollectWindowReports(nsGlobalWindow *aWindow,
                     nsWindowSizes *aWindowTotalSizes,
                     nsTHashtable<nsUint64HashKey> *aGhostWindowIDs,
                     nsIMemoryMultiReporterCallback *aCb,
                     nsISupports *aClosure)
{
  nsCAutoString windowPath("explicit/window-objects/");

  // Avoid calling aWindow->GetTop() if there's no outer window.  It will work
  // just fine, but will spew a lot of warnings.
  nsGlobalWindow *top = NULL;
  if (aWindow->GetOuterWindow()) {
    // Our window should have a null top iff it has a null docshell.
    MOZ_ASSERT(!!aWindow->GetTop() == !!aWindow->GetDocShell());
    top = aWindow->GetTop();
  }

  if (top) {
    windowPath += NS_LITERAL_CSTRING("top(");
    AppendWindowURI(top, windowPath);
    windowPath += NS_LITERAL_CSTRING(", id=");
    windowPath.AppendInt(top->WindowID());
    windowPath += NS_LITERAL_CSTRING(")/");

    windowPath += aWindow->IsFrozen() ? NS_LITERAL_CSTRING("cached/")
                                      : NS_LITERAL_CSTRING("active/");
  } else {
    if (aGhostWindowIDs->Contains(aWindow->WindowID())) {
      windowPath += NS_LITERAL_CSTRING("top(none)/ghost/");
    } else {
      windowPath += NS_LITERAL_CSTRING("top(none)/detached/");
    }
  }

  windowPath += NS_LITERAL_CSTRING("window(");
  AppendWindowURI(aWindow, windowPath);
  windowPath += NS_LITERAL_CSTRING(")");

#define REPORT(_pathTail, _amount, _desc)                                     \
  do {                                                                        \
    if (_amount > 0) {                                                        \
        nsCAutoString path(windowPath);                                       \
        path += _pathTail;                                                    \
        nsresult rv;                                                          \
        rv = aCb->Callback(EmptyCString(), path, nsIMemoryReporter::KIND_HEAP,\
                      nsIMemoryReporter::UNITS_BYTES, _amount,                \
                      NS_LITERAL_CSTRING(_desc), aClosure);                   \
        NS_ENSURE_SUCCESS(rv, rv);                                            \
    }                                                                         \
  } while (0)

  nsWindowSizes windowSizes(DOMStyleMallocSizeOf);
  aWindow->SizeOfIncludingThis(&windowSizes);

  REPORT("/dom", windowSizes.mDOM,
         "Memory used by a window and the DOM within it.");
  aWindowTotalSizes->mDOM += windowSizes.mDOM;

  REPORT("/style-sheets", windowSizes.mStyleSheets,
         "Memory used by style sheets within a window.");
  aWindowTotalSizes->mStyleSheets += windowSizes.mStyleSheets;

  REPORT("/layout/arenas", windowSizes.mLayoutArenas,
         "Memory used by layout PresShell, PresContext, and other related "
         "areas within a window.");
  aWindowTotalSizes->mLayoutArenas += windowSizes.mLayoutArenas;

  REPORT("/layout/style-sets", windowSizes.mLayoutStyleSets,
         "Memory used by style sets within a window.");
  aWindowTotalSizes->mLayoutStyleSets += windowSizes.mLayoutStyleSets;

  REPORT("/layout/text-runs", windowSizes.mLayoutTextRuns,
         "Memory used for text-runs (glyph layout) in the PresShell's frame "
         "tree, within a window.");
  aWindowTotalSizes->mLayoutTextRuns += windowSizes.mLayoutTextRuns;

#undef REPORT

  return NS_OK;
}

typedef nsTArray< nsRefPtr<nsGlobalWindow> > WindowArray;

static
PLDHashOperator
GetWindows(const PRUint64& aId, nsGlobalWindow*& aWindow, void* aClosure)
{
  ((WindowArray *)aClosure)->AppendElement(aWindow);

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsWindowMemoryReporter::GetName(nsACString &aName)
{
  aName.AssignLiteral("window-objects");
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::CollectReports(nsIMemoryMultiReporterCallback* aCb,
                                       nsISupports* aClosure)
{
  nsGlobalWindow::WindowByIdTable* windowsById =
    nsGlobalWindow::GetWindowsTable();
  NS_ENSURE_TRUE(windowsById, NS_OK);

  // Hold on to every window in memory so that window objects can't be
  // destroyed while we're calling the memory reporter callback.
  WindowArray windows;
  windowsById->Enumerate(GetWindows, &windows);

  // Get the IDs of all the "ghost" windows.
  nsTHashtable<nsUint64HashKey> ghostWindows;
  ghostWindows.Init();
  CheckForGhostWindows(&ghostWindows);

  nsCOMPtr<nsIEffectiveTLDService> tldService = do_GetService(
    NS_EFFECTIVETLDSERVICE_CONTRACTID);
  NS_ENSURE_STATE(tldService);

  // Collect window memory usage.
  nsWindowSizes windowTotalSizes(NULL);
  for (PRUint32 i = 0; i < windows.Length(); i++) {
    nsresult rv = CollectWindowReports(windows[i], &windowTotalSizes,
                                       &ghostWindows, aCb, aClosure);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#define REPORT(_path, _amount, _desc)                                         \
  do {                                                                        \
    nsresult rv;                                                              \
    rv = aCb->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path),             \
                       nsIMemoryReporter::KIND_OTHER,                         \
                       nsIMemoryReporter::UNITS_BYTES, _amount,               \
                       NS_LITERAL_CSTRING(_desc), aClosure);                  \
    NS_ENSURE_SUCCESS(rv, rv);                                                \
  } while (0)

  REPORT("window-objects-dom", windowTotalSizes.mDOM, 
         "Memory used for the DOM within windows. "
         "This is the sum of all windows' 'dom' numbers.");
    
  REPORT("window-objects-style-sheets", windowTotalSizes.mStyleSheets, 
         "Memory used for style sheets within windows. "
         "This is the sum of all windows' 'style-sheets' numbers.");
    
  REPORT("window-objects-layout-arenas", windowTotalSizes.mLayoutArenas, 
         "Memory used by layout PresShell, PresContext, and other related "
         "areas within windows. This is the sum of all windows' "
         "'layout/arenas' numbers.");
    
  REPORT("window-objects-layout-style-sets", windowTotalSizes.mLayoutStyleSets, 
         "Memory used for style sets within windows. "
         "This is the sum of all windows' 'layout/style-sets' numbers.");
    
  REPORT("window-objects-layout-text-runs", windowTotalSizes.mLayoutTextRuns, 
         "Memory used for text runs within windows. "
         "This is the sum of all windows' 'layout/text-runs' numbers.");

#undef REPORT
    
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::GetExplicitNonHeap(PRInt64* aAmount)
{
  // This reporter only measures heap memory.
  *aAmount = 0;
  return NS_OK;
}

PRUint32
nsWindowMemoryReporter::GetGhostTimeout()
{
  return Preferences::GetUint("memory.ghost_window_timeout_seconds", 60);
}

NS_IMETHODIMP
nsWindowMemoryReporter::Observe(nsISupports *aSubject, const char *aTopic,
                                const PRUnichar *aData)
{
  if (!strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC)) {
    ObserveDOMWindowDetached(aSubject);
  } else if (!strcmp(aTopic, "after-minimize-memory-usage")) {
    ObserveAfterMinimizeMemoryUsage();
  } else {
    MOZ_ASSERT(false);
  }

  return NS_OK;
}

void
nsWindowMemoryReporter::ObserveDOMWindowDetached(nsISupports* aWindow)
{
  nsWeakPtr weakWindow = do_GetWeakReference(aWindow);
  if (!weakWindow) {
    NS_WARNING("Couldn't take weak reference to a window?");
    return;
  }

  mDetachedWindows.Put(weakWindow, TimeStamp());

  if (!mCheckForGhostWindowsCallbackPending) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this,
                           &nsWindowMemoryReporter::CheckForGhostWindowsCallback);
    NS_DispatchToCurrentThread(runnable);
    mCheckForGhostWindowsCallbackPending = true;
  }
}

static PLDHashOperator
BackdateTimeStampsEnumerator(nsISupports *aKey, TimeStamp &aTimeStamp,
                             void* aClosure)
{
  TimeStamp *minTimeStamp = static_cast<TimeStamp*>(aClosure);
  
  if (!aTimeStamp.IsNull() && aTimeStamp > *minTimeStamp) {
    aTimeStamp = *minTimeStamp;
  }

  return PL_DHASH_NEXT;
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

  mDetachedWindows.Enumerate(BackdateTimeStampsEnumerator,
                             &minTimeStamp);
}

void
nsWindowMemoryReporter::CheckForGhostWindowsCallback()
{
  mCheckForGhostWindowsCallbackPending = false;
  CheckForGhostWindows();
}

struct CheckForGhostWindowsEnumeratorData
{
  nsTHashtable<nsCStringHashKey> *nonDetachedDomains;
  nsTHashtable<nsUint64HashKey> *ghostWindowIDs;
  nsIEffectiveTLDService *tldService;
  PRUint32 ghostTimeout;
  TimeStamp now;
};

static PLDHashOperator
CheckForGhostWindowsEnumerator(nsISupports *aKey, TimeStamp& aTimeStamp,
                               void* aClosure)
{
  CheckForGhostWindowsEnumeratorData *data =
    static_cast<CheckForGhostWindowsEnumeratorData*>(aClosure);

  nsWeakPtr weakKey = do_QueryInterface(aKey);
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(weakKey);
  if (!window) {
    // The window object has been destroyed.  Stop tracking its weak ref in our
    // hashtable.
    return PL_DHASH_REMOVE;
  }

  // Avoid calling GetTop() if we have no outer window.  Nothing will break if
  // we do, but it will spew debug output, which can cause our test logs to
  // overflow.
  nsCOMPtr<nsIDOMWindow> top;
  if (window->GetOuterWindow()) {
    window->GetTop(getter_AddRefs(top));
  }

  if (top) {
    // The window is no longer detached, so we no longer want to track it.
    return PL_DHASH_REMOVE;
  }

  nsCOMPtr<nsIURI> uri = GetWindowURI(window);

  nsCAutoString domain;
  if (uri) {
    // GetBaseDomain works fine if |uri| is null, but it outputs a warning
    // which ends up overrunning the mochitest logs.
    data->tldService->GetBaseDomain(uri, 0, domain);
  }

  if (data->nonDetachedDomains->Contains(domain)) {
    // This window shares a domain with a non-detached window, so reset its
    // clock.
    aTimeStamp = TimeStamp();
  } else {
    // This window does not share a domain with a non-detached window, so it
    // meets ghost criterion (2).
    if (aTimeStamp.IsNull()) {
      // This may become a ghost window later; start its clock.
      aTimeStamp = data->now;
    } else if ((data->now - aTimeStamp).ToSeconds() > data->ghostTimeout) {
      // This definitely is a ghost window, so add it to ghostWindowIDs, if
      // that is not null.
      if (data->ghostWindowIDs) {
        nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(window);
        if (pWindow) {
          data->ghostWindowIDs->PutEntry(pWindow->WindowID());
        }
      }
    }
  }

  return PL_DHASH_NEXT;
}

struct GetNonDetachedWindowDomainsEnumeratorData
{
  nsTHashtable<nsCStringHashKey> *nonDetachedDomains;
  nsIEffectiveTLDService *tldService;
};

static PLDHashOperator
GetNonDetachedWindowDomainsEnumerator(const PRUint64& aId, nsGlobalWindow* aWindow,
                                      void* aClosure)
{
  GetNonDetachedWindowDomainsEnumeratorData *data =
    static_cast<GetNonDetachedWindowDomainsEnumeratorData*>(aClosure);

  // Null outer window implies null top, but calling GetTop() when there's no
  // outer window causes us to spew debug warnings.
  if (!aWindow->GetOuterWindow() || !aWindow->GetTop()) {
    // This window is detached, so we don't care about its domain.
    return PL_DHASH_NEXT;
  }

  nsCOMPtr<nsIURI> uri = GetWindowURI(aWindow);

  nsCAutoString domain;
  if (uri) {
    data->tldService->GetBaseDomain(uri, 0, domain);
  }

  data->nonDetachedDomains->PutEntry(domain);
  return PL_DHASH_NEXT;
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
  nsTHashtable<nsUint64HashKey> *aOutGhostIDs /* = NULL */)
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

  nsTHashtable<nsCStringHashKey> nonDetachedWindowDomains;
  nonDetachedWindowDomains.Init();

  // Populate nonDetachedWindowDomains.
  GetNonDetachedWindowDomainsEnumeratorData nonDetachedEnumData =
    { &nonDetachedWindowDomains, tldService };
  windowsById->EnumerateRead(GetNonDetachedWindowDomainsEnumerator,
                             &nonDetachedEnumData);

  // Update mDetachedWindows and write the ghost window IDs into aOutGhostIDs,
  // if it's not null.
  CheckForGhostWindowsEnumeratorData ghostEnumData =
    { &nonDetachedWindowDomains, aOutGhostIDs, tldService,
      GetGhostTimeout(), TimeStamp::Now() };
  mDetachedWindows.Enumerate(CheckForGhostWindowsEnumerator,
                             &ghostEnumData);
}

NS_IMPL_ISUPPORTS1(nsWindowMemoryReporter::GhostURLsReporter,
                   nsIMemoryMultiReporter)

nsWindowMemoryReporter::
GhostURLsReporter::GhostURLsReporter(
  nsWindowMemoryReporter* aWindowReporter)
  : mWindowReporter(aWindowReporter)
{
}

NS_IMETHODIMP
nsWindowMemoryReporter::
GhostURLsReporter::GetName(nsACString& aName)
{
  aName.AssignLiteral("ghost-windows");
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::
GhostURLsReporter::GetExplicitNonHeap(PRInt64* aOut)
{
  *aOut = 0;
  return NS_OK;
}

struct ReportGhostWindowsEnumeratorData
{
  nsIMemoryMultiReporterCallback* callback;
  nsISupports* closure;
  nsresult rv;
};

static PLDHashOperator
ReportGhostWindowsEnumerator(nsUint64HashKey* aIDHashKey, void* aClosure)
{
  ReportGhostWindowsEnumeratorData *data =
    static_cast<ReportGhostWindowsEnumeratorData*>(aClosure);

  nsGlobalWindow::WindowByIdTable* windowsById =
    nsGlobalWindow::GetWindowsTable();
  if (!windowsById) {
    NS_WARNING("Couldn't get window-by-id hashtable?");
    return PL_DHASH_NEXT;
  }

  nsGlobalWindow* window = windowsById->Get(aIDHashKey->GetKey());
  if (!window) {
    NS_WARNING("Could not look up window?");
    return PL_DHASH_NEXT;
  }

  nsCAutoString path;
  path.AppendLiteral("ghost-windows/");
  AppendWindowURI(window, path);

  nsresult rv = data->callback->Callback(
    /* process = */ EmptyCString(),
    path,
    nsIMemoryReporter::KIND_SUMMARY,
    nsIMemoryReporter::UNITS_COUNT,
    /* amount = */ 1,
    /* desc = */ EmptyCString(),
    data->closure);

  if (NS_FAILED(rv) && NS_SUCCEEDED(data->rv)) {
    data->rv = rv;
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsWindowMemoryReporter::
GhostURLsReporter::CollectReports(
  nsIMemoryMultiReporterCallback* aCb,
  nsISupports* aClosure)
{
  // Get the IDs of all the ghost windows in existance.
  nsTHashtable<nsUint64HashKey> ghostWindows;
  ghostWindows.Init();
  mWindowReporter->CheckForGhostWindows(&ghostWindows);

  ReportGhostWindowsEnumeratorData reportGhostWindowsEnumData =
    { aCb, aClosure, NS_OK };

  // Call aCb->Callback() for each ghost window.
  ghostWindows.EnumerateEntries(ReportGhostWindowsEnumerator,
                                &reportGhostWindowsEnumData);

  return reportGhostWindowsEnumData.rv;
}

NS_IMPL_ISUPPORTS1(nsWindowMemoryReporter::NumGhostsReporter,
                   nsIMemoryReporter)

nsWindowMemoryReporter::
NumGhostsReporter::NumGhostsReporter(
  nsWindowMemoryReporter *aWindowReporter)
  : mWindowReporter(aWindowReporter)
{}

NS_IMETHODIMP
nsWindowMemoryReporter::
NumGhostsReporter::GetProcess(nsACString& aProcess)
{
  aProcess.AssignLiteral("");
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::
NumGhostsReporter::GetPath(nsACString& aPath)
{
  aPath.AssignLiteral("ghost-windows");
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::
NumGhostsReporter::GetKind(PRInt32* aKind)
{
  *aKind = KIND_OTHER;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::
NumGhostsReporter::GetUnits(PRInt32* aUnits)
{
  *aUnits = nsIMemoryReporter::UNITS_COUNT;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::
NumGhostsReporter::GetDescription(nsACString& aDesc)
{
  nsPrintfCString str(
"The number of ghost windows present (the number of nodes underneath \
explicit/window-objects/top(none)/ghost, modulo race conditions).  A ghost \
window is not shown in any tab, does not share a domain with any non-detached \
windows, and has met these criteria for at least %ds \
(memory.ghost_window_timeout_seconds) or has survived a round of about:memory's \
minimize memory usage button.\n\n\
Ghost windows can happen legitimately, but they are often indicative of leaks \
in the browser or add-ons.",
  mWindowReporter->GetGhostTimeout());

  aDesc.Assign(str);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowMemoryReporter::
NumGhostsReporter::GetAmount(PRInt64* aAmount)
{
  nsTHashtable<nsUint64HashKey> ghostWindows;
  ghostWindows.Init();
  mWindowReporter->CheckForGhostWindows(&ghostWindows);

  *aAmount = ghostWindows.Count();
  return NS_OK;
}
