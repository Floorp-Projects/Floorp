/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PictureInPicture: "resource://gre/modules/PictureInPicture.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTabWarmingEnabled",
  "browser.tabs.remote.warmup.enabled"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTabWarmingMax",
  "browser.tabs.remote.warmup.maxTabs"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTabWarmingUnloadDelayMs",
  "browser.tabs.remote.warmup.unloadDelayMs"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTabCacheSize",
  "browser.tabs.remote.tabCacheSize"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTabUnloadDelay",
  "browser.tabs.remote.unloadDelayMs",
  300
);

// Floorp Injections
function floorpSplitViewIsEnabled() {
  return Services.prefs.getBoolPref("floorp.browser.splitView.working", false);
}

/**
 * The tab switcher is responsible for asynchronously switching
 * tabs in e10s. It waits until the new tab is ready (i.e., the
 * layer tree is available) before switching to it. Then it
 * unloads the layer tree for the old tab.
 *
 * The tab switcher is a state machine. For each tab, it
 * maintains state about whether the layer tree for the tab is
 * available, being loaded, being unloaded, or unavailable. It
 * also keeps track of the tab currently being displayed, the tab
 * it's trying to load, and the tab the user has asked to switch
 * to. The switcher object is created upon tab switch. It is
 * released when there are no pending tabs to load or unload.
 *
 * The following general principles have guided the design:
 *
 * 1. We only request one layer tree at a time. If the user
 * switches to a different tab while waiting, we don't request
 * the new layer tree until the old tab has loaded or timed out.
 *
 * 2. If loading the layers for a tab times out, we show the
 * spinner and possibly request the layer tree for another tab if
 * the user has requested one.
 *
 * 3. We discard layer trees on a delay. This way, if the user is
 * switching among the same tabs frequently, we don't continually
 * load the same tabs.
 *
 * It's important that we always show either the spinner or a tab
 * whose layers are available. Otherwise the compositor will draw
 * an entirely black frame, which is very jarring. To ensure this
 * never happens when switching away from a tab, we assume the
 * old tab might still be drawn until a MozAfterPaint event
 * occurs. Because layout and compositing happen asynchronously,
 * we don't have any other way of knowing when the switch
 * actually takes place. Therefore, we don't unload the old tab
 * until the next MozAfterPaint event.
 */
export class AsyncTabSwitcher {
  constructor(tabbrowser) {
    this.log("START");

    // How long to wait for a tab's layers to load. After this
    // time elapses, we're free to put up the spinner and start
    // trying to load a different tab.
    this.TAB_SWITCH_TIMEOUT = 400; // ms

    // When the user hasn't switched tabs for this long, we unload
    // layers for all tabs that aren't in use.
    this.UNLOAD_DELAY = lazy.gTabUnloadDelay; // ms

    // The next three tabs form the principal state variables.
    // See the assertions in postActions for their invariants.

    // Tab the user requested most recently.
    this.requestedTab = tabbrowser.selectedTab;

    // Tab we're currently trying to load.
    this.loadingTab = null;

    // We show this tab in case the requestedTab hasn't loaded yet.
    this.lastVisibleTab = tabbrowser.selectedTab;

    // Auxilliary state variables:

    this.visibleTab = tabbrowser.selectedTab; // Tab that's on screen.
    this.spinnerTab = null; // Tab showing a spinner.
    this.blankTab = null; // Tab showing blank.
    this.lastPrimaryTab = tabbrowser.selectedTab; // Tab with primary="true"

    this.tabbrowser = tabbrowser;
    this.window = tabbrowser.ownerGlobal;
    this.loadTimer = null; // TAB_SWITCH_TIMEOUT nsITimer instance.
    this.unloadTimer = null; // UNLOAD_DELAY nsITimer instance.

    // Map from tabs to STATE_* (below).
    this.tabState = new Map();

    // True if we're in the midst of switching tabs.
    this.switchInProgress = false;

    // Transaction id for the composite that will show the requested
    // tab for the first tab after a tab switch.
    // Set to -1 when we're not waiting for notification of a
    // completed switch.
    this.switchPaintId = -1;

    // Set of tabs that might be visible right now. We maintain
    // this set because we can't be sure when a tab is actually
    // drawn. A tab is added to this set when we ask to make it
    // visible. All tabs but the most recently shown tab are
    // removed from the set upon MozAfterPaint.
    this.maybeVisibleTabs = new Set([tabbrowser.selectedTab]);

    // This holds onto the set of tabs that we've been asked to warm up,
    // and tabs are evicted once they're done loading or are unloaded.
    this.warmingTabs = new WeakSet();

    this.STATE_UNLOADED = 0;
    this.STATE_LOADING = 1;
    this.STATE_LOADED = 2;
    this.STATE_UNLOADING = 3;

    // re-entrancy guard:
    this._processing = false;

    // For telemetry, keeps track of what most recently cleared
    // the loadTimer, which can tell us something about the cause
    // of tab switch spinners.
    this._loadTimerClearedBy = "none";

    this._useDumpForLogging = false;
    this._logInit = false;
    this._logFlags = [];

    this.window.addEventListener("MozAfterPaint", this);
    this.window.addEventListener("MozLayerTreeReady", this);
    this.window.addEventListener("MozLayerTreeCleared", this);
    this.window.addEventListener("TabRemotenessChange", this);
    this.window.addEventListener("SwapDocShells", this, true);
    this.window.addEventListener("EndSwapDocShells", this, true);
    this.window.document.addEventListener("visibilitychange", this);

    let initialTab = this.requestedTab;
    let initialBrowser = initialTab.linkedBrowser;

    let tabIsLoaded =
      !initialBrowser.isRemoteBrowser ||
      initialBrowser.frameLoader.remoteTab?.hasLayers;

    // If we minimized the window before the switcher was activated,
    // we might have set  the preserveLayers flag for the current
    // browser. Let's clear it.
    initialBrowser.preserveLayers(false);

    if (!this.windowHidden) {
      this.log("Initial tab is loaded?: " + tabIsLoaded);
      this.setTabState(
        initialTab,
        tabIsLoaded ? this.STATE_LOADED : this.STATE_LOADING
      );
    }

    for (let ppBrowser of this.tabbrowser._printPreviewBrowsers) {
      let ppTab = this.tabbrowser.getTabForBrowser(ppBrowser);
      let state = ppBrowser.hasLayers ? this.STATE_LOADED : this.STATE_LOADING;
      this.setTabState(ppTab, state);
    }
  }

  destroy() {
    if (this.unloadTimer) {
      this.clearTimer(this.unloadTimer);
      this.unloadTimer = null;
    }
    if (this.loadTimer) {
      this.clearTimer(this.loadTimer);
      this.loadTimer = null;
    }

    this.window.removeEventListener("MozAfterPaint", this);
    this.window.removeEventListener("MozLayerTreeReady", this);
    this.window.removeEventListener("MozLayerTreeCleared", this);
    this.window.removeEventListener("TabRemotenessChange", this);
    this.window.removeEventListener("SwapDocShells", this, true);
    this.window.removeEventListener("EndSwapDocShells", this, true);
    this.window.document.removeEventListener("visibilitychange", this);

    this.tabbrowser._switcher = null;
  }

  // Wraps nsITimer. Must not use the vanilla setTimeout and
  // clearTimeout, because they will be blocked by nsIPromptService
  // dialogs.
  setTimer(callback, timeout) {
    let event = {
      notify: callback,
    };

    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(event, timeout, Ci.nsITimer.TYPE_ONE_SHOT);
    return timer;
  }

  clearTimer(timer) {
    timer.cancel();
  }

  getTabState(tab) {
    let state = this.tabState.get(tab);

    // As an optimization, we lazily evaluate the state of tabs
    // that we've never seen before. Once we've figured it out,
    // we stash it in our state map.
    if (state === undefined) {
      state = this.STATE_UNLOADED;

      if (tab && tab.linkedPanel) {
        let b = tab.linkedBrowser;
        if (b.renderLayers && b.hasLayers) {
          state = this.STATE_LOADED;
        } else if (b.renderLayers && !b.hasLayers) {
          state = this.STATE_LOADING;
        } else if (!b.renderLayers && b.hasLayers) {
          state = this.STATE_UNLOADING;
        }
      }

      this.setTabStateNoAction(tab, state);
    }

    return state;
  }

  setTabStateNoAction(tab, state) {
    if (state == this.STATE_UNLOADED) {
      this.tabState.delete(tab);
    } else {
      this.tabState.set(tab, state);
    }
  }

  setTabState(tab, state) {
    if (state == this.getTabState(tab)) {
      return;
    }

    this.setTabStateNoAction(tab, state);

    let browser = tab.linkedBrowser;
    let { remoteTab } = browser.frameLoader;
    if (state == this.STATE_LOADING) {
      this.assert(!this.windowHidden);

      // If we're not in the process of warming this tab, we
      // don't need to delay activating its DocShell.
      if (!this.warmingTabs.has(tab)) {
        browser.docShellIsActive = true;
      }

      // Floorp Injections
      if (remoteTab && !floorpSplitViewIsEnabled()) {
        browser.renderLayers = true;
        remoteTab.priorityHint = true;
      }
      if (browser.hasLayers) {
        this.onLayersReady(browser);
      }
    } else if (state == this.STATE_UNLOADING) {
      this.unwarmTab(tab);
      // Setting the docShell to be inactive will also cause it
      // to stop rendering layers.
      browser.docShellIsActive = false;
      // Floorp Injections
      if (remoteTab && !floorpSplitViewIsEnabled()) {
        remoteTab.priorityHint = false;
      }
      if (!browser.hasLayers) {
        this.onLayersCleared(browser);
      }
    } else if (state == this.STATE_LOADED) {
      this.maybeActivateDocShell(tab);
    }

    if (!tab.linkedBrowser.isRemoteBrowser) {
      // setTabState is potentially re-entrant, so we must re-get the state for
      // this assertion.
      let nonRemoteState = this.getTabState(tab);
      // Non-remote tabs can never stay in the STATE_LOADING
      // or STATE_UNLOADING states. By the time this function
      // exits, a non-remote tab must be in STATE_LOADED or
      // STATE_UNLOADED, since the painting and the layer
      // upload happen synchronously.
      this.assert(
        nonRemoteState == this.STATE_UNLOADED ||
          nonRemoteState == this.STATE_LOADED
      );
    }
  }

  get windowHidden() {
    return this.window.document.hidden;
  }

  get tabLayerCache() {
    return this.tabbrowser._tabLayerCache;
  }

  finish() {
    this.log("FINISH");

    this.assert(this.tabbrowser._switcher);
    this.assert(this.tabbrowser._switcher === this);
    this.assert(!this.spinnerTab);
    this.assert(!this.blankTab);
    this.assert(!this.loadTimer);
    this.assert(!this.loadingTab);
    this.assert(this.lastVisibleTab === this.requestedTab);
    this.assert(
      this.windowHidden ||
        this.getTabState(this.requestedTab) == this.STATE_LOADED
    );

    this.destroy();

    this.window.document.commandDispatcher.unlock();

    let event = new this.window.CustomEvent("TabSwitchDone", {
      bubbles: true,
      cancelable: true,
    });
    this.tabbrowser.dispatchEvent(event);
  }

  // This function is called after all the main state changes to
  // make sure we display the right tab.
  updateDisplay() {
    let requestedTabState = this.getTabState(this.requestedTab);
    let requestedBrowser = this.requestedTab.linkedBrowser;

    // It is often more desirable to show a blank tab when appropriate than
    // the tab switch spinner - especially since the spinner is usually
    // preceded by a perceived lag of TAB_SWITCH_TIMEOUT ms in the
    // tab switch. We can hide this lag, and hide the time being spent
    // constructing BrowserChild's, layer trees, etc, by showing a blank
    // tab instead and focusing it immediately.
    let shouldBeBlank = false;
    // Floorp Injections
    if (requestedBrowser.isRemoteBrowser && !floorpSplitViewIsEnabled()) {
      // If a tab is remote and the window is not minimized, we can show a
      // blank tab instead of a spinner in the following cases:
      //
      // 1. The tab has just crashed, and we haven't started showing the
      //    tab crashed page yet (in this case, the RemoteTab is null)
      // 2. The tab has never presented, and has not finished loading
      //    a non-local-about: page.
      //
      // For (2), "finished loading a non-local-about: page" is
      // determined by the busy state on the tab element and checking
      // if the loaded URI is local.
      let isBusy = this.requestedTab.hasAttribute("busy");
      let isLocalAbout = requestedBrowser.currentURI.schemeIs("about");
      let hasSufficientlyLoaded = !isBusy && !isLocalAbout;

      let fl = requestedBrowser.frameLoader;
      shouldBeBlank =
        !this.windowHidden &&
        (!fl.remoteTab ||
          (!hasSufficientlyLoaded && !fl.remoteTab.hasPresented));

      if (this.logging()) {
        let flag = shouldBeBlank ? "blank" : "nonblank";
        this.addLogFlag(
          flag,
          this.windowHidden,
          fl.remoteTab,
          isBusy,
          isLocalAbout,
          fl.remoteTab ? fl.remoteTab.hasPresented : 0
        );
      }
    }

    // Floorp Injections
    if (requestedBrowser.isRemoteBrowser && !floorpSplitViewIsEnabled()) {
      this.addLogFlag("isRemote");
    }

    // Figure out which tab we actually want visible right now.
    let showTab = null;
    if (
      requestedTabState != this.STATE_LOADED &&
      this.lastVisibleTab &&
      this.loadTimer &&
      !shouldBeBlank
    ) {
      // If we can't show the requestedTab, and lastVisibleTab is
      // available, show it.
      showTab = this.lastVisibleTab;
    } else {
      // Show the requested tab. If it's not available, we'll show the spinner or a blank tab.
      showTab = this.requestedTab;
    }

    // First, let's deal with blank tabs, which we show instead
    // of the spinner when the tab is not currently set up
    // properly in the content process.
    if (!shouldBeBlank && this.blankTab) {
      this.blankTab.linkedBrowser.removeAttribute("blank");
      this.blankTab = null;
    } else if (shouldBeBlank && this.blankTab !== showTab) {
      if (this.blankTab) {
        this.blankTab.linkedBrowser.removeAttribute("blank");
      }
      this.blankTab = showTab;
      this.blankTab.linkedBrowser.setAttribute("blank", "true");
    }

    // Show or hide the spinner as needed.
    let needSpinner =
      this.getTabState(showTab) != this.STATE_LOADED &&
      !this.windowHidden &&
      !shouldBeBlank &&
      !this.loadTimer;

    if (!needSpinner && this.spinnerTab) {
      this.noteSpinnerHidden();
      this.tabbrowser.tabpanels.removeAttribute("pendingpaint");
      this.spinnerTab.linkedBrowser.removeAttribute("pendingpaint");
      this.spinnerTab = null;
    } else if (needSpinner && this.spinnerTab !== showTab) {
      if (this.spinnerTab) {
        this.spinnerTab.linkedBrowser.removeAttribute("pendingpaint");
      } else {
        this.noteSpinnerDisplayed();
      }
      this.spinnerTab = showTab;
      this.tabbrowser.tabpanels.toggleAttribute("pendingpaint", true);
      this.spinnerTab.linkedBrowser.toggleAttribute("pendingpaint", true);
    }

    // Switch to the tab we've decided to make visible.
    if (this.visibleTab !== showTab) {
      this.tabbrowser._adjustFocusBeforeTabSwitch(this.visibleTab, showTab);
      this.visibleTab = showTab;

      this.maybeVisibleTabs.add(showTab);

      let tabpanels = this.tabbrowser.tabpanels;
      let showPanel = this.tabbrowser.tabContainer.getRelatedElement(showTab);
      let index = Array.prototype.indexOf.call(tabpanels.children, showPanel);
      if (index != -1) {
        this.log(`Switch to tab ${index} - ${this.tinfo(showTab)}`);
        tabpanels.updateSelectedIndex(index);
        if (showTab === this.requestedTab) {
          if (requestedTabState == this.STATE_LOADED) {
            // The new tab will be made visible in the next paint, record the expected
            // transaction id for that, and we'll mark when we get notified of its
            // completion.
            this.switchPaintId = this.window.windowUtils.lastTransactionId + 1;
          } else {
            this.noteMakingTabVisibleWithoutLayers();
          }

          this.tabbrowser._adjustFocusAfterTabSwitch(showTab);
          this.window.gURLBar.afterTabSwitchFocusChange();
          this.maybeActivateDocShell(this.requestedTab);
        }
      }

      // This doesn't necessarily exist if we're a new window and haven't switched tabs yet
      if (this.lastVisibleTab) {
        this.lastVisibleTab._visuallySelected = false;
      }

      this.visibleTab._visuallySelected = true;
    }

    this.lastVisibleTab = this.visibleTab;
  }

  assert(cond) {
    if (!cond) {
      dump("Assertion failure\n" + Error().stack);

      // Don't break a user's browser if an assertion fails.
      if (AppConstants.DEBUG) {
        throw new Error("Assertion failure");
      }
    }
  }

  maybeClearLoadTimer(caller) {
    if (this.loadingTab) {
      this._loadTimerClearedBy = caller;
      this.loadingTab = null;
      if (this.loadTimer) {
        this.clearTimer(this.loadTimer);
        this.loadTimer = null;
      }
    }
  }

  // We've decided to try to load requestedTab.
  loadRequestedTab() {
    this.assert(!this.loadTimer);
    this.assert(!this.windowHidden);

    // loadingTab can be non-null here if we timed out loading the current tab.
    // In that case we just overwrite it with a different tab; it's had its chance.
    this.loadingTab = this.requestedTab;
    this.log("Loading tab " + this.tinfo(this.loadingTab));

    this.loadTimer = this.setTimer(
      () => this.handleEvent({ type: "loadTimeout" }),
      this.TAB_SWITCH_TIMEOUT
    );
    this.setTabState(this.requestedTab, this.STATE_LOADING);
  }

  maybeActivateDocShell(tab) {
    // If we've reached the point where the requested tab has entered
    // the loaded state, but the DocShell is still not yet active, we
    // should activate it.
    let browser = tab.linkedBrowser;
    let state = this.getTabState(tab);
    let canCheckDocShellState =
      !browser.mDestroyed &&
      (browser.docShell || browser.frameLoader.remoteTab);
    if (
      tab == this.requestedTab &&
      canCheckDocShellState &&
      state == this.STATE_LOADED &&
      !browser.docShellIsActive &&
      !this.windowHidden
    ) {
      browser.docShellIsActive = true;
      this.logState(
        "Set requested tab docshell to active and preserveLayers to false"
      );
      // If we minimized the window before the switcher was activated,
      // we might have set the preserveLayers flag for the current
      // browser. Let's clear it.
      browser.preserveLayers(false);
    }
  }

  // This function runs before every event. It fixes up the state
  // to account for closed tabs.
  preActions() {
    this.assert(this.tabbrowser._switcher);
    this.assert(this.tabbrowser._switcher === this);

    for (let i = 0; i < this.tabLayerCache.length; i++) {
      let tab = this.tabLayerCache[i];
      if (!tab.linkedBrowser) {
        this.tabState.delete(tab);
        this.tabLayerCache.splice(i, 1);
        i--;
      }
    }

    for (let [tab] of this.tabState) {
      if (!tab.linkedBrowser) {
        this.tabState.delete(tab);
        this.unwarmTab(tab);
      }
    }

    if (this.lastVisibleTab && !this.lastVisibleTab.linkedBrowser) {
      this.lastVisibleTab = null;
    }
    if (this.lastPrimaryTab && !this.lastPrimaryTab.linkedBrowser) {
      this.lastPrimaryTab = null;
    }
    if (this.blankTab && !this.blankTab.linkedBrowser) {
      this.blankTab = null;
    }
    if (this.spinnerTab && !this.spinnerTab.linkedBrowser) {
      this.noteSpinnerHidden();
      this.spinnerTab = null;
    }
    if (this.loadingTab && !this.loadingTab.linkedBrowser) {
      this.maybeClearLoadTimer("preActions");
    }
  }

  // This code runs after we've responded to an event or requested a new
  // tab. It's expected that we've already updated all the principal
  // state variables. This function takes care of updating any auxilliary
  // state.
  postActions(eventString) {
    // Once we finish loading loadingTab, we null it out. So the state should
    // always be LOADING.
    this.assert(
      !this.loadingTab ||
        this.getTabState(this.loadingTab) == this.STATE_LOADING
    );

    // We guarantee that loadingTab is non-null iff loadTimer is non-null. So
    // the timer is set only when we're loading something.
    this.assert(!this.loadTimer || this.loadingTab);
    this.assert(!this.loadingTab || this.loadTimer);

    // If we're switching to a non-remote tab, there's no need to wait
    // for it to send layers to the compositor, as this will happen
    // synchronously. Clearing this here means that in the next step,
    // we can load the non-remote browser immediately.
    if (!this.requestedTab.linkedBrowser.isRemoteBrowser) {
      this.maybeClearLoadTimer("postActions");
    }

    // If we're not loading anything, try loading the requested tab.
    let stateOfRequestedTab = this.getTabState(this.requestedTab);
    if (
      !this.loadTimer &&
      !this.windowHidden &&
      (stateOfRequestedTab == this.STATE_UNLOADED ||
        stateOfRequestedTab == this.STATE_UNLOADING ||
        this.warmingTabs.has(this.requestedTab))
    ) {
      this.assert(stateOfRequestedTab != this.STATE_LOADED);
      this.loadRequestedTab();
    }

    let numBackgroundCached = 0;
    for (let tab of this.tabLayerCache) {
      if (tab !== this.requestedTab) {
        numBackgroundCached++;
      }
    }

    // See how many tabs still have work to do.
    let numPending = 0;
    let numWarming = 0;
    for (let [tab, state] of this.tabState) {
      if (!this.shouldDeactivateDocShell(tab.linkedBrowser)) {
        continue;
      }

      if (
        state == this.STATE_LOADED &&
        tab !== this.requestedTab &&
        !this.tabLayerCache.includes(tab)
      ) {
        numPending++;

        if (tab !== this.visibleTab) {
          numWarming++;
        }
      }
      if (state == this.STATE_LOADING || state == this.STATE_UNLOADING) {
        numPending++;
      }
    }

    this.updateDisplay();

    // It's possible for updateDisplay to trigger one of our own event
    // handlers, which might cause finish() to already have been called.
    // Check for that before calling finish() again.
    if (!this.tabbrowser._switcher) {
      return;
    }

    this.maybeFinishTabSwitch();

    if (numBackgroundCached > 0) {
      this.deactivateCachedBackgroundTabs();
    }

    if (numWarming > lazy.gTabWarmingMax) {
      this.logState("Hit tabWarmingMax");
      if (this.unloadTimer) {
        this.clearTimer(this.unloadTimer);
      }
      this.unloadNonRequiredTabs();
    }

    if (numPending == 0) {
      this.finish();
    }

    this.logState("/" + eventString);
  }

  // Fires when we're ready to unload unused tabs.
  onUnloadTimeout() {
    this.unloadTimer = null;
    this.unloadNonRequiredTabs();
  }

  deactivateCachedBackgroundTabs() {
    for (let tab of this.tabLayerCache) {
      if (tab !== this.requestedTab) {
        let browser = tab.linkedBrowser;
        browser.preserveLayers(true);
        browser.docShellIsActive = false;
      }
    }
  }

  // If there are any non-visible and non-requested tabs in
  // STATE_LOADED, sets them to STATE_UNLOADING. Also queues
  // up the unloadTimer to run onUnloadTimeout if there are still
  // tabs in the process of unloading.
  unloadNonRequiredTabs() {
    this.warmingTabs = new WeakSet();
    let numPending = 0;

    // Unload any tabs that can be unloaded.
    for (let [tab, state] of this.tabState) {
      if (!this.shouldDeactivateDocShell(tab.linkedBrowser)) {
        continue;
      }

      let isInLayerCache = this.tabLayerCache.includes(tab);

      if (
        state == this.STATE_LOADED &&
        !this.maybeVisibleTabs.has(tab) &&
        tab !== this.lastVisibleTab &&
        tab !== this.loadingTab &&
        tab !== this.requestedTab &&
        !isInLayerCache
      ) {
        this.setTabState(tab, this.STATE_UNLOADING);
      }

      if (
        state != this.STATE_UNLOADED &&
        tab !== this.requestedTab &&
        !isInLayerCache
      ) {
        numPending++;
      }
    }

    if (numPending) {
      // Keep the timer going since there may be more tabs to unload.
      this.unloadTimer = this.setTimer(
        () => this.handleEvent({ type: "unloadTimeout" }),
        this.UNLOAD_DELAY
      );
    }
  }

  // Fires when an ongoing load has taken too long.
  onLoadTimeout() {
    this.maybeClearLoadTimer("onLoadTimeout");
  }

  // Fires when the layers become available for a tab.
  onLayersReady(browser) {
    let tab = this.tabbrowser.getTabForBrowser(browser);
    if (!tab) {
      // We probably got a layer update from a tab that got before
      // the switcher was created, or for browser that's not being
      // tracked by the async tab switcher (like the preloaded about:newtab).
      return;
    }

    this.logState(`onLayersReady(${tab._tPos}, ${browser.isRemoteBrowser})`);
    this.assert(
      this.getTabState(tab) == this.STATE_LOADING ||
        this.getTabState(tab) == this.STATE_LOADED
    );
    this.setTabState(tab, this.STATE_LOADED);
    this.unwarmTab(tab);

    if (this.loadingTab === tab) {
      this.maybeClearLoadTimer("onLayersReady");
    }
  }

  // Fires when we paint the screen. Any tab switches we initiated
  // previously are done, so there's no need to keep the old layers
  // around.
  onPaint(event) {
    this.addLogFlag(
      "onPaint",
      this.switchPaintId != -1,
      event.transactionId >= this.switchPaintId
    );
    this.notePaint(event);
    this.maybeVisibleTabs.clear();
  }

  // Called when we're done clearing the layers for a tab.
  onLayersCleared(browser) {
    let tab = this.tabbrowser.getTabForBrowser(browser);
    if (!tab) {
      return;
    }
    this.logState(`onLayersCleared(${tab._tPos})`);
    this.assert(
      this.getTabState(tab) == this.STATE_UNLOADING ||
        this.getTabState(tab) == this.STATE_UNLOADED
    );
    this.setTabState(tab, this.STATE_UNLOADED);
  }

  // Called when a tab switches from remote to non-remote. In this case
  // a MozLayerTreeReady notification that we requested may never fire,
  // so we need to simulate it.
  onRemotenessChange(tab) {
    this.logState(
      `onRemotenessChange(${tab._tPos}, ${tab.linkedBrowser.isRemoteBrowser})`
    );
    if (!tab.linkedBrowser.isRemoteBrowser) {
      // Floorp Injections
      if (this.getTabState(tab) == this.STATE_LOADING && !floorpSplitViewIsEnabled()) {
        this.onLayersReady(tab.linkedBrowser);
      } else if (this.getTabState(tab) == this.STATE_UNLOADING) {
        this.onLayersCleared(tab.linkedBrowser);
      }
    } else if (this.getTabState(tab) == this.STATE_LOADED) {
      // A tab just changed from non-remote to remote, which means
      // that it's gone back into the STATE_LOADING state until
      // it sends up a layer tree.
      this.setTabState(tab, this.STATE_LOADING);
    }
  }

  onTabRemoved(tab) {
    if (this.lastVisibleTab == tab) {
      this.handleEvent({ type: "tabRemoved", tab });
    }
  }

  // Called when a tab has been removed, and the browser node is
  // about to be removed from the DOM.
  onTabRemovedImpl() {
    this.lastVisibleTab = null;
  }

  onVisibilityChange() {
    if (this.windowHidden) {
      for (let [tab, state] of this.tabState) {
        if (!this.shouldDeactivateDocShell(tab.linkedBrowser)) {
          continue;
        }

        if (state == this.STATE_LOADING || state == this.STATE_LOADED) {
          this.setTabState(tab, this.STATE_UNLOADING);
        }
      }
      this.maybeClearLoadTimer("onSizeModeOrOcc");
    } else {
      // We're no longer minimized or occluded. This means we might want
      // to activate the current tab's docShell.
      this.maybeActivateDocShell(this.tabbrowser.selectedTab);
    }
  }

  onSwapDocShells(ourBrowser, otherBrowser) {
    // This event fires before the swap. ourBrowser is from
    // our window. We save the state of otherBrowser since ourBrowser
    // needs to take on that state at the end of the swap.

    let otherTabbrowser = otherBrowser.ownerGlobal.gBrowser;
    let otherState;
    if (otherTabbrowser && otherTabbrowser._switcher) {
      let otherTab = otherTabbrowser.getTabForBrowser(otherBrowser);
      let otherSwitcher = otherTabbrowser._switcher;
      otherState = otherSwitcher.getTabState(otherTab);
    } else {
      otherState = otherBrowser.docShellIsActive
        ? this.STATE_LOADED
        : this.STATE_UNLOADED;
    }
    if (!this.swapMap) {
      this.swapMap = new WeakMap();
    }
    this.swapMap.set(otherBrowser, {
      state: otherState,
    });
  }

  onEndSwapDocShells(ourBrowser, otherBrowser) {
    // The swap has happened. We reset the loadingTab in
    // case it has been swapped. We also set ourBrowser's state
    // to whatever otherBrowser's state was before the swap.

    // Clearing the load timer means that we will
    // immediately display a spinner if ourBrowser isn't
    // ready yet. Typically it will already be ready
    // though. If it's not, we're probably in a new window,
    // in which case we have no other tabs to display anyway.
    this.maybeClearLoadTimer("onEndSwapDocShells");

    let { state: otherState } = this.swapMap.get(otherBrowser);

    this.swapMap.delete(otherBrowser);

    let ourTab = this.tabbrowser.getTabForBrowser(ourBrowser);
    if (ourTab) {
      this.setTabStateNoAction(ourTab, otherState);
    }
  }

  /**
   * Check if the browser should be deactivated. If the browser is a print preivew or
   * PiP browser then we won't deactive it.
   * @param browser The browser to check if it should be deactivated
   * @returns false if a print preview or PiP browser else true
   */
  shouldDeactivateDocShell(browser) {
    return !(
      this.tabbrowser._printPreviewBrowsers.has(browser) ||
      lazy.PictureInPicture.isOriginatingBrowser(browser)
    );
  }

  shouldActivateDocShell(browser) {
    let tab = this.tabbrowser.getTabForBrowser(browser);
    let state = this.getTabState(tab);
    return state == this.STATE_LOADING || state == this.STATE_LOADED;
  }

  activateBrowserForPrintPreview(browser) {
    let tab = this.tabbrowser.getTabForBrowser(browser);
    let state = this.getTabState(tab);
    if (state != this.STATE_LOADING && state != this.STATE_LOADED) {
      this.setTabState(tab, this.STATE_LOADING);
      this.logState(
        "Activated browser " + this.tinfo(tab) + " for print preview"
      );
    }
  }

  canWarmTab(tab) {
    if (!lazy.gTabWarmingEnabled) {
      return false;
    }

    if (!tab) {
      return false;
    }

    // If the tab is not yet inserted, closing, not remote,
    // crashed, already visible, or already requested, warming
    // up the tab makes no sense.
    if (
      this.windowHidden ||
      !tab.linkedPanel ||
      tab.closing ||
      !tab.linkedBrowser.isRemoteBrowser ||
      !tab.linkedBrowser.frameLoader.remoteTab
    ) {
      return false;
    }

    return true;
  }

  shouldWarmTab(tab) {
    if (this.canWarmTab(tab)) {
      // Tabs that are already in STATE_LOADING or STATE_LOADED
      // have no need to be warmed up.
      let state = this.getTabState(tab);
      if (state === this.STATE_UNLOADING || state === this.STATE_UNLOADED) {
        return true;
      }
    }

    return false;
  }

  unwarmTab(tab) {
    this.warmingTabs.delete(tab);
  }

  warmupTab(tab) {
    if (!this.shouldWarmTab(tab)) {
      return;
    }

    this.logState("warmupTab " + this.tinfo(tab));

    this.warmingTabs.add(tab);
    this.setTabState(tab, this.STATE_LOADING);
    this.queueUnload(lazy.gTabWarmingUnloadDelayMs);
  }

  cleanUpTabAfterEviction(tab) {
    this.assert(tab !== this.requestedTab);
    let browser = tab.linkedBrowser;
    if (browser) {
      browser.preserveLayers(false);
    }
    this.setTabState(tab, this.STATE_UNLOADING);
  }

  evictOldestTabFromCache() {
    let tab = this.tabLayerCache.shift();
    this.cleanUpTabAfterEviction(tab);
  }

  maybePromoteTabInLayerCache(tab) {
    if (
      lazy.gTabCacheSize > 1 &&
      tab.linkedBrowser.isRemoteBrowser &&
      tab.linkedBrowser.currentURI.spec != "about:blank"
      // Floorp Injections
      && !floorpSplitViewIsEnabled()
    ) {
      let tabIndex = this.tabLayerCache.indexOf(tab);

      if (tabIndex != -1) {
        this.tabLayerCache.splice(tabIndex, 1);
      }

      this.tabLayerCache.push(tab);

      if (this.tabLayerCache.length > lazy.gTabCacheSize) {
        this.evictOldestTabFromCache();
      }
    }
  }

  // Called when the user asks to switch to a given tab.
  requestTab(tab) {
    if (tab === this.requestedTab) {
      return;
    }

    let tabState = this.getTabState(tab);
    this.noteTabRequested(tab, tabState);

    this.logState("requestTab " + this.tinfo(tab));
    this.startTabSwitch();

    let oldBrowser = this.requestedTab.linkedBrowser;
    oldBrowser.deprioritize();
    this.requestedTab = tab;
    if (tabState == this.STATE_LOADED) {
      this.maybeVisibleTabs.clear();
    }

    tab.linkedBrowser.setAttribute("primary", "true");
    if (this.lastPrimaryTab && this.lastPrimaryTab != tab) {
      this.lastPrimaryTab.linkedBrowser.removeAttribute("primary");
    }
    this.lastPrimaryTab = tab;

    this.queueUnload(this.UNLOAD_DELAY);
  }

  queueUnload(unloadTimeout) {
    this.handleEvent({ type: "queueUnload", unloadTimeout });
  }

  onQueueUnload(unloadTimeout) {
    if (this.unloadTimer) {
      this.clearTimer(this.unloadTimer);
    }
    this.unloadTimer = this.setTimer(
      () => this.handleEvent({ type: "unloadTimeout" }),
      unloadTimeout
    );
  }

  handleEvent(event, delayed = false) {
    if (this._processing) {
      this.setTimer(() => this.handleEvent(event, true), 0);
      return;
    }
    if (delayed && this.tabbrowser._switcher != this) {
      // if we delayed processing this event, we might be out of date, in which
      // case we drop the delayed events
      return;
    }
    this._processing = true;
    try {
      this.preActions();

      switch (event.type) {
        case "queueUnload":
          this.onQueueUnload(event.unloadTimeout);
          break;
        case "unloadTimeout":
          this.onUnloadTimeout();
          break;
        case "loadTimeout":
          this.onLoadTimeout();
          break;
        case "tabRemoved":
          this.onTabRemovedImpl(event.tab);
          break;
        case "MozLayerTreeReady": {
          let browser = event.originalTarget;
          if (!browser.renderLayers) {
            // By the time we handle this event, it's possible that something
            // else has already set renderLayers to false, in which case this
            // event is stale and we can safely ignore it.
            return;
          }
          this.onLayersReady(browser);
          break;
        }
        case "MozAfterPaint":
          this.onPaint(event);
          break;
        case "MozLayerTreeCleared": {
          let browser = event.originalTarget;
          if (browser.renderLayers) {
            // By the time we handle this event, it's possible that something
            // else has already set renderLayers to true, in which case this
            // event is stale and we can safely ignore it.
            return;
          }
          this.onLayersCleared(browser);
          break;
        }
        case "TabRemotenessChange":
          this.onRemotenessChange(event.target);
          break;
        case "visibilitychange":
          this.onVisibilityChange();
          break;
        case "SwapDocShells":
          this.onSwapDocShells(event.originalTarget, event.detail);
          break;
        case "EndSwapDocShells":
          this.onEndSwapDocShells(event.originalTarget, event.detail);
          break;
      }

      this.postActions(event.type);
    } finally {
      this._processing = false;
    }
  }

  /*
   * Telemetry and Profiler related helpers for recording tab switch
   * timing.
   */

  startTabSwitch() {
    this.noteStartTabSwitch();
    this.switchInProgress = true;
  }

  /**
   * Something has occurred that might mean that we've completed
   * the tab switch (layers are ready, paints are done, spinners
   * are hidden). This checks to make sure all conditions are
   * satisfied, and then records the tab switch as finished.
   */
  maybeFinishTabSwitch() {
    if (
      this.switchInProgress &&
      this.requestedTab &&
      (this.getTabState(this.requestedTab) == this.STATE_LOADED ||
        this.requestedTab === this.blankTab)
    ) {
      if (this.requestedTab !== this.blankTab) {
        this.maybePromoteTabInLayerCache(this.requestedTab);
      }

      this.noteFinishTabSwitch();
      this.switchInProgress = false;

      let event = new this.window.CustomEvent("TabSwitched", {
        bubbles: true,
        detail: {
          tab: this.requestedTab,
        },
      });
      this.tabbrowser.dispatchEvent(event);
    }
  }

  /*
   * Debug related logging for switcher.
   */
  logging() {
    if (this._useDumpForLogging) {
      return true;
    }
    if (this._logInit) {
      return this._shouldLog;
    }
    let result = Services.prefs.getBoolPref(
      "browser.tabs.remote.logSwitchTiming",
      false
    );
    this._shouldLog = result;
    this._logInit = true;
    return this._shouldLog;
  }

  tinfo(tab) {
    if (tab) {
      return tab._tPos + "(" + tab.linkedBrowser.currentURI.spec + ")";
    }
    return "null";
  }

  log(s) {
    if (!this.logging()) {
      return;
    }
    if (this._useDumpForLogging) {
      dump(s + "\n");
    } else {
      Services.console.logStringMessage(s);
    }
  }

  addLogFlag(flag, ...subFlags) {
    if (this.logging()) {
      if (subFlags.length) {
        flag += `(${subFlags.map(f => (f ? 1 : 0)).join("")})`;
      }
      this._logFlags.push(flag);
    }
  }

  logState(suffix) {
    if (!this.logging()) {
      return;
    }

    let getTabString = tab => {
      let tabString = "";

      let state = this.getTabState(tab);
      let isWarming = this.warmingTabs.has(tab);
      let isCached = this.tabLayerCache.includes(tab);
      let isClosing = tab.closing;
      let linkedBrowser = tab.linkedBrowser;
      let isActive = linkedBrowser && linkedBrowser.docShellIsActive;
      let isRendered = linkedBrowser && linkedBrowser.renderLayers;
      let isPiP =
        linkedBrowser &&
        lazy.PictureInPicture.isOriginatingBrowser(linkedBrowser);

      if (tab === this.lastVisibleTab) {
        tabString += "V";
      }
      if (tab === this.loadingTab) {
        tabString += "L";
      }
      if (tab === this.requestedTab) {
        tabString += "R";
      }
      if (tab === this.blankTab) {
        tabString += "B";
      }
      if (this.maybeVisibleTabs.has(tab)) {
        tabString += "M";
      }

      let extraStates = "";
      if (isWarming) {
        extraStates += "W";
      }
      if (isCached) {
        extraStates += "C";
      }
      if (isClosing) {
        extraStates += "X";
      }
      if (isActive) {
        extraStates += "A";
      }
      if (isRendered) {
        extraStates += "R";
      }
      if (isPiP) {
        extraStates += "P";
      }
      if (extraStates != "") {
        tabString += `(${extraStates})`;
      }

      switch (state) {
        case this.STATE_LOADED: {
          tabString += "(loaded)";
          break;
        }
        case this.STATE_LOADING: {
          tabString += "(loading)";
          break;
        }
        case this.STATE_UNLOADING: {
          tabString += "(unloading)";
          break;
        }
        case this.STATE_UNLOADED: {
          tabString += "(unloaded)";
          break;
        }
      }

      return tabString;
    };

    let accum = "";

    // This is a bit tricky to read, but what we're doing here is collapsing
    // identical tab states down to make the overal string shorter and easier
    // to read, and we move all simply unloaded tabs to the back of the list.
    // I.e., we turn
    //   "0:(unloaded) 1:(unloaded) 2:(unloaded) 3:(loaded)""
    // into
    //   "3:(loaded) 0...2:(unloaded)"
    let tabStrings = this.tabbrowser.tabs.map(t => getTabString(t));
    let lastMatch = -1;
    let unloadedTabsStrings = [];
    for (let i = 0; i <= tabStrings.length; i++) {
      if (i > 0) {
        if (i < tabStrings.length && tabStrings[i] == tabStrings[lastMatch]) {
          continue;
        }

        if (tabStrings[lastMatch] == "(unloaded)") {
          if (lastMatch == i - 1) {
            unloadedTabsStrings.push(lastMatch.toString());
          } else {
            unloadedTabsStrings.push(`${lastMatch}...${i - 1}`);
          }
        } else if (lastMatch == i - 1) {
          accum += `${lastMatch}:${tabStrings[lastMatch]} `;
        } else {
          accum += `${lastMatch}...${i - 1}:${tabStrings[lastMatch]} `;
        }
      }

      lastMatch = i;
    }

    if (unloadedTabsStrings.length) {
      accum += `${unloadedTabsStrings.join(",")}:(unloaded) `;
    }

    accum += "cached: " + this.tabLayerCache.length + " ";

    if (this._logFlags.length) {
      accum += `[${this._logFlags.join(",")}] `;
      this._logFlags = [];
    }

    // It can be annoying to read through the entirety of a log string just
    // to check if something changed or not. So if we can tell that nothing
    // changed, just write "unchanged" to save the reader's time.
    let logString;
    if (this._lastLogString == accum) {
      accum = "unchanged";
    } else {
      this._lastLogString = accum;
    }
    logString = `ATS: ${accum}{${suffix}}`;

    if (this._useDumpForLogging) {
      dump(logString + "\n");
    } else {
      Services.console.logStringMessage(logString);
    }
  }

  noteMakingTabVisibleWithoutLayers() {
    // We're making the tab visible even though we haven't yet got layers for it.
    // It's hard to know which composite the layers will first be available in (and
    // the parent process might not even get MozAfterPaint delivered for it), so just
    // give up measuring this for now. :(
    Glean.performanceInteraction.tabSwitchComposite.cancel(
      this._tabswitchTimerId
    );
    this._tabswitchTimerId = null;
  }

  notePaint(event) {
    if (this.switchPaintId != -1 && event.transactionId >= this.switchPaintId) {
      if (this._tabswitchTimerId) {
        Glean.performanceInteraction.tabSwitchComposite.stopAndAccumulate(
          this._tabswitchTimerId
        );
        this._tabswitchTimerId = null;
      }
      let { innerWindowId } = this.window.windowGlobalChild;
      ChromeUtils.addProfilerMarker("AsyncTabSwitch:Composited", {
        innerWindowId,
      });
      this.switchPaintId = -1;
    }
  }

  noteTabRequested(tab, tabState) {
    if (lazy.gTabWarmingEnabled) {
      let warmingState = "disqualified";

      if (this.canWarmTab(tab)) {
        if (tabState == this.STATE_LOADING) {
          warmingState = "stillLoading";
        } else if (tabState == this.STATE_LOADED) {
          warmingState = "loaded";
        } else if (
          tabState == this.STATE_UNLOADING ||
          tabState == this.STATE_UNLOADED
        ) {
          // At this point, if the tab's browser was being inserted
          // lazily, we never had a chance to warm it up, and unfortunately
          // there's no great way to detect that case. Those cases will
          // end up in the "notWarmed" bucket, along with legitimate cases
          // where tabs could have been warmed but weren't.
          warmingState = "notWarmed";
        }
      }

      Services.telemetry
        .getHistogramById("FX_TAB_SWITCH_REQUEST_TAB_WARMING_STATE")
        .add(warmingState);
    }
  }

  noteStartTabSwitch() {
    TelemetryStopwatch.cancel("FX_TAB_SWITCH_TOTAL_E10S_MS", this.window);
    TelemetryStopwatch.start("FX_TAB_SWITCH_TOTAL_E10S_MS", this.window);

    if (this._tabswitchTimerId) {
      Glean.performanceInteraction.tabSwitchComposite.cancel(
        this._tabswitchTimerId
      );
    }
    this._tabswitchTimerId =
      Glean.performanceInteraction.tabSwitchComposite.start();
    let { innerWindowId } = this.window.windowGlobalChild;
    ChromeUtils.addProfilerMarker("AsyncTabSwitch:Start", { innerWindowId });
  }

  noteFinishTabSwitch() {
    // After this point the tab has switched from the content thread's point of view.
    // The changes will be visible after the next refresh driver tick + composite.
    let time = TelemetryStopwatch.timeElapsed(
      "FX_TAB_SWITCH_TOTAL_E10S_MS",
      this.window
    );
    if (time != -1) {
      TelemetryStopwatch.finish("FX_TAB_SWITCH_TOTAL_E10S_MS", this.window);
      this.log("DEBUG: tab switch time = " + time);
      let { innerWindowId } = this.window.windowGlobalChild;
      ChromeUtils.addProfilerMarker("AsyncTabSwitch:Finish", { innerWindowId });
    }
  }

  noteSpinnerDisplayed() {
    this.assert(!this.spinnerTab);
    let browser = this.requestedTab.linkedBrowser;
    this.assert(browser.isRemoteBrowser);
    TelemetryStopwatch.start("FX_TAB_SWITCH_SPINNER_VISIBLE_MS", this.window);
    // We have a second, similar probe for capturing recordings of
    // when the spinner is displayed for very long periods.
    TelemetryStopwatch.start(
      "FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS",
      this.window
    );
    let { innerWindowId } = this.window.windowGlobalChild;
    ChromeUtils.addProfilerMarker("AsyncTabSwitch:SpinnerShown", {
      innerWindowId,
    });
    Services.telemetry
      .getHistogramById("FX_TAB_SWITCH_SPINNER_VISIBLE_TRIGGER")
      .add(this._loadTimerClearedBy);
    if (AppConstants.NIGHTLY_BUILD) {
      Services.obs.notifyObservers(null, "tabswitch-spinner");
    }
  }

  noteSpinnerHidden() {
    this.assert(this.spinnerTab);
    this.log(
      "DEBUG: spinner time = " +
        TelemetryStopwatch.timeElapsed(
          "FX_TAB_SWITCH_SPINNER_VISIBLE_MS",
          this.window
        )
    );
    TelemetryStopwatch.finish("FX_TAB_SWITCH_SPINNER_VISIBLE_MS", this.window);
    TelemetryStopwatch.finish(
      "FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS",
      this.window
    );
    let { innerWindowId } = this.window.windowGlobalChild;
    ChromeUtils.addProfilerMarker("AsyncTabSwitch:SpinnerHidden", {
      innerWindowId,
    });
    // we do not get a onPaint after displaying the spinner
    this._loadTimerClearedBy = "none";
  }
}
