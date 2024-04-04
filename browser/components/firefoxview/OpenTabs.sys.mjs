/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module provides the means to monitor and query for tab collections against open
 * browser windows and allow listeners to be notified of changes to those collections.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

const TAB_ATTRS_TO_WATCH = Object.freeze([
  "attention",
  "image",
  "label",
  "muted",
  "soundplaying",
  "titlechanged",
]);
const TAB_CHANGE_EVENTS = Object.freeze([
  "TabAttrModified",
  "TabClose",
  "TabMove",
  "TabOpen",
  "TabPinned",
  "TabUnpinned",
]);
const TAB_RECENCY_CHANGE_EVENTS = Object.freeze([
  "activate",
  "sizemodechange",
  "TabAttrModified",
  "TabClose",
  "TabOpen",
  "TabPinned",
  "TabUnpinned",
  "TabSelect",
  "TabAttrModified",
]);

// Debounce tab/tab recency changes and dispatch max once per frame at 60fps
const CHANGES_DEBOUNCE_MS = 1000 / 60;

/**
 * A sort function used to order tabs by most-recently seen and active.
 */
export function lastSeenActiveSort(a, b) {
  let dt = b.lastSeenActive - a.lastSeenActive;
  if (dt) {
    return dt;
  }
  // try to break a deadlock by sorting the selected tab higher
  if (!(a.selected || b.selected)) {
    return 0;
  }
  return a.selected ? -1 : 1;
}

/**
 * Provides a object capable of monitoring and accessing tab collections for either
 * private or non-private browser windows. As the class extends EventTarget, consumers
 * should add event listeners for the change events.
 *
 * @param {boolean} options.usePrivateWindows
              Constrain to only windows that match this privateness. Defaults to false.
 * @param {Window | null} options.exclusiveWindow
 *            Constrain to only a specific window.
 */
class OpenTabsTarget extends EventTarget {
  #changedWindowsByType = {
    TabChange: new Set(),
    TabRecencyChange: new Set(),
  };
  #sourceEventsByType = {
    TabChange: new Set(),
    TabRecencyChange: new Set(),
  };
  #dispatchChangesTask;
  #started = false;
  #watchedWindows = new Set();

  #exclusiveWindowWeakRef = null;
  usePrivateWindows = false;

  constructor(options = {}) {
    super();
    this.usePrivateWindows = !!options.usePrivateWindows;

    if (options.exclusiveWindow) {
      this.exclusiveWindow = options.exclusiveWindow;
      this.everyWindowCallbackId = `opentabs-${this.exclusiveWindow.windowGlobalChild.innerWindowId}`;
    } else {
      this.everyWindowCallbackId = `opentabs-${
        this.usePrivateWindows ? "private" : "non-private"
      }`;
    }
  }

  get exclusiveWindow() {
    return this.#exclusiveWindowWeakRef?.get();
  }
  set exclusiveWindow(newValue) {
    if (newValue) {
      this.#exclusiveWindowWeakRef = Cu.getWeakReference(newValue);
    } else {
      this.#exclusiveWindowWeakRef = null;
    }
  }

  includeWindowFilter(win) {
    if (this.#exclusiveWindowWeakRef) {
      return win == this.exclusiveWindow;
    }
    return (
      win.gBrowser &&
      !win.closed &&
      this.usePrivateWindows == lazy.PrivateBrowsingUtils.isWindowPrivate(win)
    );
  }

  get currentWindows() {
    return lazy.EveryWindow.readyWindows.filter(win =>
      this.includeWindowFilter(win)
    );
  }

  /**
   * A promise that resolves to all matched windows once their delayedStartupPromise resolves
   */
  get readyWindowsPromise() {
    let windowList = Array.from(
      Services.wm.getEnumerator("navigator:browser")
    ).filter(win => {
      // avoid waiting for windows we definitely don't care about
      if (this.#exclusiveWindowWeakRef) {
        return this.exclusiveWindow == win;
      }
      return (
        this.usePrivateWindows == lazy.PrivateBrowsingUtils.isWindowPrivate(win)
      );
    });
    return Promise.allSettled(
      windowList.map(win => win.delayedStartupPromise)
    ).then(() => {
      // re-filter the list as properties might have changed in the interim
      return windowList.filter(() => this.includeWindowFilter);
    });
  }

  haveListenersForEvent(eventType) {
    switch (eventType) {
      case "TabChange":
        return Services.els.hasListenersFor(this, "TabChange");
      case "TabRecencyChange":
        return Services.els.hasListenersFor(this, "TabRecencyChange");
      default:
        return false;
    }
  }

  get haveAnyListeners() {
    return (
      this.haveListenersForEvent("TabChange") ||
      this.haveListenersForEvent("TabRecencyChange")
    );
  }

  /*
   * @param {string} type
   *        Either "TabChange" or "TabRecencyChange"
   * @param {Object|Function} listener
   * @param {Object} [options]
   */
  addEventListener(type, listener, options) {
    let hadListeners = this.haveAnyListeners;
    super.addEventListener(type, listener, options);

    // if this is the first listener, start up all the window & tab monitoring
    if (!hadListeners && this.haveAnyListeners) {
      this.start();
    }
  }

  /*
   * @param {string} type
   *        Either "TabChange" or "TabRecencyChange"
   * @param {Object|Function} listener
   */
  removeEventListener(type, listener) {
    let hadListeners = this.haveAnyListeners;
    super.removeEventListener(type, listener);

    // if this was the last listener, we can stop all the window & tab monitoring
    if (hadListeners && !this.haveAnyListeners) {
      this.stop();
    }
  }

  /**
   * Begin watching for tab-related events from all browser windows matching the instance's private property
   */
  start() {
    if (this.#started) {
      return;
    }
    // EveryWindow will call #watchWindow for each open window once its delayedStartupPromise resolves.
    lazy.EveryWindow.registerCallback(
      this.everyWindowCallbackId,
      win => this.#watchWindow(win),
      win => this.#unwatchWindow(win)
    );
    this.#started = true;
  }

  /**
   * Stop watching for tab-related events from all browser windows and clean up.
   */
  stop() {
    if (this.#started) {
      lazy.EveryWindow.unregisterCallback(this.everyWindowCallbackId);
      this.#started = false;
    }
    for (let changedWindows of Object.values(this.#changedWindowsByType)) {
      changedWindows.clear();
    }
    for (let sourceEvents of Object.values(this.#sourceEventsByType)) {
      sourceEvents.clear();
    }
    this.#watchedWindows.clear();
    this.#dispatchChangesTask?.disarm();
  }

  /**
   * Add listeners for tab-related events from the given window. The consumer's
   * listeners will always be notified at least once for newly-watched window.
   */
  #watchWindow(win) {
    if (!this.includeWindowFilter(win)) {
      return;
    }
    this.#watchedWindows.add(win);
    const { tabContainer } = win.gBrowser;
    tabContainer.addEventListener("TabAttrModified", this);
    tabContainer.addEventListener("TabClose", this);
    tabContainer.addEventListener("TabMove", this);
    tabContainer.addEventListener("TabOpen", this);
    tabContainer.addEventListener("TabPinned", this);
    tabContainer.addEventListener("TabUnpinned", this);
    tabContainer.addEventListener("TabSelect", this);
    win.addEventListener("activate", this);
    win.addEventListener("sizemodechange", this);

    this.#scheduleEventDispatch("TabChange", {
      sourceWindowId: win.windowGlobalChild.innerWindowId,
      sourceEvent: "watchWindow",
    });
    this.#scheduleEventDispatch("TabRecencyChange", {
      sourceWindowId: win.windowGlobalChild.innerWindowId,
      sourceEvent: "watchWindow",
    });
  }

  /**
   * Remove all listeners for tab-related events from the given window.
   * Consumers will always be notified at least once for unwatched window.
   */
  #unwatchWindow(win) {
    // We check the window is in our watchedWindows collection rather than currentWindows
    // as the unwatched window may not match the criteria we used to watch it anymore,
    // and we need to unhook our event listeners regardless.
    if (this.#watchedWindows.has(win)) {
      this.#watchedWindows.delete(win);

      const { tabContainer } = win.gBrowser;
      tabContainer.removeEventListener("TabAttrModified", this);
      tabContainer.removeEventListener("TabClose", this);
      tabContainer.removeEventListener("TabMove", this);
      tabContainer.removeEventListener("TabOpen", this);
      tabContainer.removeEventListener("TabPinned", this);
      tabContainer.removeEventListener("TabSelect", this);
      tabContainer.removeEventListener("TabUnpinned", this);
      win.removeEventListener("activate", this);
      win.removeEventListener("sizemodechange", this);

      this.#scheduleEventDispatch("TabChange", {
        sourceWindowId: win.windowGlobalChild.innerWindowId,
        sourceEvent: "unwatchWindow",
      });
      this.#scheduleEventDispatch("TabRecencyChange", {
        sourceWindowId: win.windowGlobalChild.innerWindowId,
        sourceEvent: "unwatchWindow",
      });
    }
  }

  /**
   * Flag the need to notify all our consumers of a change to open tabs.
   * Repeated calls within approx 16ms will be consolidated
   * into one event dispatch.
   */
  #scheduleEventDispatch(eventType, { sourceWindowId, sourceEvent } = {}) {
    if (!this.haveListenersForEvent(eventType)) {
      return;
    }

    this.#sourceEventsByType[eventType].add(sourceEvent);
    this.#changedWindowsByType[eventType].add(sourceWindowId);
    // Queue up an event dispatch - we use a deferred task to make this less noisy by
    // consolidating multiple change events into one.
    if (!this.#dispatchChangesTask) {
      this.#dispatchChangesTask = new lazy.DeferredTask(() => {
        this.#dispatchChanges();
      }, CHANGES_DEBOUNCE_MS);
    }
    this.#dispatchChangesTask.arm();
  }

  #dispatchChanges() {
    this.#dispatchChangesTask?.disarm();
    for (let [eventType, changedWindowIds] of Object.entries(
      this.#changedWindowsByType
    )) {
      let sourceEvents = this.#sourceEventsByType[eventType];
      if (this.haveListenersForEvent(eventType) && changedWindowIds.size) {
        let changeEvent = new CustomEvent(eventType, {
          detail: {
            windowIds: [...changedWindowIds],
            sourceEvents: [...sourceEvents],
          },
        });
        this.dispatchEvent(changeEvent);
        changedWindowIds.clear();
      }
      sourceEvents?.clear();
    }
  }

  /*
   * @param {Window} win
   * @param {boolean} sortByRecency
   * @returns {Array<Tab>}
   *    The list of visible tabs for the browser window
   */
  getTabsForWindow(win, sortByRecency = false) {
    if (this.currentWindows.includes(win)) {
      const { visibleTabs } = win.gBrowser;
      return sortByRecency
        ? visibleTabs.toSorted(lastSeenActiveSort)
        : [...visibleTabs];
    }
    return [];
  }

  /**
   * Get an aggregated list of tabs from all the same-privateness browser windows.
   *
   * @returns {MozTabbrowserTab[]}
   */
  getAllTabs() {
    return this.currentWindows.flatMap(win => this.getTabsForWindow(win));
  }

  /*
   * @returns {Array<Tab>}
   *    A by-recency-sorted, aggregated list of tabs from all the same-privateness browser windows.
   */
  getRecentTabs() {
    return this.getAllTabs().sort(lastSeenActiveSort);
  }

  handleEvent({ detail, target, type }) {
    const win = target.ownerGlobal;
    // NOTE: we already filtered on privateness by not listening for those events
    // from private/not-private windows
    if (
      type == "TabAttrModified" &&
      !detail.changed.some(attr => TAB_ATTRS_TO_WATCH.includes(attr))
    ) {
      return;
    }

    if (TAB_RECENCY_CHANGE_EVENTS.includes(type)) {
      this.#scheduleEventDispatch("TabRecencyChange", {
        sourceWindowId: win.windowGlobalChild.innerWindowId,
        sourceEvent: type,
      });
    }
    if (TAB_CHANGE_EVENTS.includes(type)) {
      this.#scheduleEventDispatch("TabChange", {
        sourceWindowId: win.windowGlobalChild.innerWindowId,
        sourceEvent: type,
      });
    }
  }
}

const gExclusiveWindows = new (class {
  perWindowInstances = new WeakMap();
  constructor() {
    Services.obs.addObserver(this, "domwindowclosed");
  }
  observe(subject) {
    let win = subject;
    let winTarget = this.perWindowInstances.get(win);
    if (winTarget) {
      winTarget.stop();
      this.perWindowInstances.delete(win);
    }
  }
})();

/**
 * Get an OpenTabsTarget instance constrained to a specific window.
 *
 * @param {Window} exclusiveWindow
 * @returns {OpenTabsTarget}
 */
const getTabsTargetForWindow = function (exclusiveWindow) {
  let instance = gExclusiveWindows.perWindowInstances.get(exclusiveWindow);
  if (instance) {
    return instance;
  }
  instance = new OpenTabsTarget({
    exclusiveWindow,
  });
  gExclusiveWindows.perWindowInstances.set(exclusiveWindow, instance);
  return instance;
};

const NonPrivateTabs = new OpenTabsTarget({
  usePrivateWindows: false,
});

const PrivateTabs = new OpenTabsTarget({
  usePrivateWindows: true,
});

export { NonPrivateTabs, PrivateTabs, getTabsTargetForWindow };
