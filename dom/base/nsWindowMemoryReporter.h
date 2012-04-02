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

#ifndef nsWindowMemoryReporter_h__
#define nsWindowMemoryReporter_h__

#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsDataHashtable.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"
#include "mozilla/TimeStamp.h"

// This should be used for any nsINode sub-class that has fields of its own
// that it needs to measure;  any sub-class that doesn't use it will inherit
// SizeOfExcludingThis from its super-class.  SizeOfIncludingThis() need not be
// defined, it is inherited from nsINode.
#define NS_DECL_SIZEOF_EXCLUDING_THIS \
  virtual size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

class nsWindowSizes {
public:
  nsWindowSizes(nsMallocSizeOfFun aMallocSizeOf) {
    memset(this, 0, sizeof(nsWindowSizes));
    mMallocSizeOf = aMallocSizeOf;
  }
  nsMallocSizeOfFun mMallocSizeOf;
  size_t mDOM;
  size_t mStyleSheets;
  size_t mLayoutArenas;
  size_t mLayoutStyleSets;
  size_t mLayoutTextRuns;
};

/**
 * nsWindowMemoryReporter is responsible for the 'explicit/window-objects'
 * memory reporter.
 *
 * We classify DOM window objects into one of three categories:
 *
 *   - "active" windows, which are displayed in a tab (as the top-level window
 *     or an iframe),
 *
 *   - "cached" windows, which are in the fastback cache (aka the bfcache), and
 *
 *   - "detached" windows, which have a null docshell.  A window becomes
 *     detached when its <iframe> or tab containing the window is destroyed --
 *     i.e., when the window is no longer active or cached.
 *
 * Additionally, we classify a subset of detached windows as "ghost" windows.
 * Although ghost windows can happen legitimately (a page can hold a reference
 * to a cross-domain window and then close its container), the presence of
 * ghost windows is often indicative of a memory leak.
 *
 * A window is a ghost if it meets the following three criteria:
 *
 *   1) The window is detached.
 *
 *   2) There exist no non-detached windows with the same base domain as
 *      the window's principal.  (For example, the base domain of
 *      "wiki.mozilla.co.uk" is "mozilla.co.uk".)  This criterion makes us less
 *      likely to flag a legitimately held-alive detached window as a ghost.
 *
 *   3) The window has met criteria (1) and (2) above for at least
 *      memory.ghost_window_timeout_seconds.  This criterion is in place so we
 *      don't immediately declare a window a ghost before the GC/CC has had a
 *      chance to run.
 *
 * nsWindowMemoryReporter observes window detachment and uses mDetachedWindows
 * to remember when a window first met criteria (1) and (2).  When we generate
 * a memory report, we use this accounting to determine which windows are
 * ghosts.
 *
 *
 * We use the following memory reporter path for active and cached windows:
 *
 *   explicit/window-objects/top(<top-outer-uri>, id=<top-outer-id>)/<category>/window(<window-uri>)/...
 *
 * For detached and ghost windows, we use
 *
 *   explicit/window-objects/top(none)/<category>/window(<window-uri>)/...
 *
 * Where
 *
 * - <category> is "active", "cached", "detached", or "ghost", as described
 *   above.
 *
 * - <top-outer-id> is the window id of the top outer window (i.e. the tab, or
 *   the top level chrome window).  Exposing this ensures that each tab gets
 *   its own sub-tree, even if multiple tabs are showing the same URI.
 *
 * - <top-uri> is the URI of the top window.  Excepting special windows (such
 *   as browser.xul or hiddenWindow.html) it's what the address bar shows for
 *   the tab.
 *
 */
class nsWindowMemoryReporter: public nsIMemoryMultiReporter,
                              public nsIObserver,
                              public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYMULTIREPORTER
  NS_DECL_NSIOBSERVER

  static void Init();

private:
  /**
   * GhostURLsReporter generates the "ghost-windows" multi-report, which
   * includes a list of all ghost windows' URLs.  If you're only interested in
   * this list, running this report is faster than running
   * nsWindowMemoryReporter.
   */
  class GhostURLsReporter: public nsIMemoryMultiReporter
  {
  public:
    GhostURLsReporter(nsWindowMemoryReporter* aWindowReporter);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORYMULTIREPORTER

  private:
    nsRefPtr<nsWindowMemoryReporter> mWindowReporter;
  };

  /**
   * nsGhostWindowReporter generates the "ghost-windows" single-report, which
   * counts the number of ghost windows present.
   */
  class NumGhostsReporter: public nsIMemoryReporter
  {
  public:
    NumGhostsReporter(nsWindowMemoryReporter* aWindowReporter);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORYREPORTER

  private:
    nsRefPtr<nsWindowMemoryReporter> mWindowReporter;
  };

  // Protect ctor, use Init() instead.
  nsWindowMemoryReporter();

  /**
   * Get the number of seconds for which a window must satisfy ghost criteria
   * (1) and (2) before we deem that it satisfies criterion (3).
   */
  PRUint32 GetGhostTimeout();

  void ObserveDOMWindowDetached(nsISupports* aWindow);
  void ObserveAfterMinimizeMemoryUsage();

  /**
   * When we observe a DOM window being detached, we enqueue an asynchronous
   * event which calls this method.  This method then calls
   * CheckForGhostWindows.
   */
  void CheckForGhostWindowsCallback();

  /**
   * Iterate over all weak window pointers in mDetachedWindows and update our
   * accounting of which windows meet ghost criterion (2).
   *
   * This method also cleans up mDetachedWindows, removing entries for windows
   * which have been destroyed or are no longer detached.
   *
   * If aOutGhostIDs is non-null, we populate it with the Window IDs of the
   * ghost windows.
   *
   * This is called asynchronously after we observe a DOM window being detached
   * from its docshell, and also right before we generate a memory report.
   */
  void CheckForGhostWindows(nsTHashtable<nsUint64HashKey> *aOutGhostIDs = NULL);

  /**
   * Maps a weak reference to a detached window (nsIWeakReference) to the time
   * when we observed that the window met ghost criterion (2) above.
   *
   * If the window has not yet met criterion (2) it maps to the null timestamp.
   *
   * (Although windows are not added to this table until they're detached, it's
   * possible for a detached window to become non-detached, and we won't
   * remove it from the table until CheckForGhostWindows runs.)
   */
  nsDataHashtable<nsISupportsHashKey, mozilla::TimeStamp> mDetachedWindows;

  /**
   * True if we have an asynchronous call to CheckForGhostWindows pending.
   */
  bool mCheckForGhostWindowsCallbackPending;
};

#endif // nsWindowMemoryReporter_h__

