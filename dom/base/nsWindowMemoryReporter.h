/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowMemoryReporter_h__
#define nsWindowMemoryReporter_h__

#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TimeStamp.h"

class nsGlobalWindowInner;

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
 *   explicit/window-objects/top(<top-outer-uri>, id=<top-outer-id>)/
 *       <category>/window(<window-uri>)/...
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
 *   as browser.xhtml or hiddenWindow.html) it's what the address bar shows for
 *   the tab.
 *
 */
class nsWindowMemoryReporter final : public nsIMemoryReporter,
                                     public nsIObserver,
                                     public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER
  NS_DECL_NSIOBSERVER

  static void Init();

#ifdef DEBUG
  /**
   * Unlink all known ghost windows, to enable investigating what caused them
   * to become ghost windows in the first place.
   */
  static void UnlinkGhostWindows();
#endif

  static nsWindowMemoryReporter* Get();
  void ObserveDOMWindowDetached(nsGlobalWindowInner* aWindow);

  static int64_t GhostWindowsDistinguishedAmount();

 private:
  ~nsWindowMemoryReporter();

  // Protect ctor, use Init() instead.
  nsWindowMemoryReporter();

  /**
   * Get the number of seconds for which a window must satisfy ghost criteria
   * (1) and (2) before we deem that it satisfies criterion (3).
   */
  uint32_t GetGhostTimeout();

  void ObserveAfterMinimizeMemoryUsage();

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
  void CheckForGhostWindows(nsTHashSet<uint64_t>* aOutGhostIDs = nullptr);

  /**
   * Eventually do a check for ghost windows, if we haven't done one recently
   * and we aren't already planning to do one soon.
   */
  void AsyncCheckForGhostWindows();

  /**
   * Kill the check timer, if it exists.
   */
  void KillCheckTimer();

  static void CheckTimerFired(nsITimer* aTimer, void* aClosure);

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
  nsTHashMap<nsISupportsHashKey, mozilla::TimeStamp> mDetachedWindows;

  /**
   * Track the last time we ran CheckForGhostWindows(), to avoid running it
   * too often after a DOM window is detached.
   */
  mozilla::TimeStamp mLastCheckForGhostWindows;

  nsCOMPtr<nsITimer> mCheckTimer;

  bool mCycleCollectorIsRunning;

  bool mCheckTimerWaitingForCCEnd;

  int64_t mGhostWindowCount;
};

#endif  // nsWindowMemoryReporter_h__
