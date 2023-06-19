/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports the FirefoxViewNotificationManager singleton, which manages the notification state
 * for the Firefox View button
 */

const RECENT_TABS_SYNC = "services.sync.lastTabFetch";
const SHOULD_NOTIFY_FOR_TABS = "browser.tabs.firefox-view.notify-for-tabs";
const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
});

export const FirefoxViewNotificationManager = new (class {
  #currentlyShowing;
  constructor() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "lastTabFetch",
      RECENT_TABS_SYNC,
      0,
      () => {
        this.handleTabSync();
      }
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "shouldNotifyForTabs",
      SHOULD_NOTIFY_FOR_TABS,
      false
    );
    // Need to access the pref variable for the observer to start observing
    // See the defineLazyPreferenceGetter function header
    this.lastTabFetch;

    Services.obs.addObserver(this, "firefoxview-notification-dot-update");

    this.#currentlyShowing = false;
  }

  async handleTabSync() {
    if (!this.shouldNotifyForTabs) {
      return;
    }
    let newSyncedTabs = await lazy.SyncedTabs.getRecentTabs(3);
    this.#currentlyShowing = this.tabsListChanged(newSyncedTabs);
    this.showNotificationDot();
    this.syncedTabs = newSyncedTabs;
  }

  showNotificationDot() {
    if (this.#currentlyShowing) {
      Services.obs.notifyObservers(
        null,
        "firefoxview-notification-dot-update",
        "true"
      );
    }
  }

  observe(sub, topic, data) {
    if (topic === "firefoxview-notification-dot-update" && data === "false") {
      this.#currentlyShowing = false;
    }
  }

  tabsListChanged(newTabs) {
    // The first time the tabs list is changed this.tabs is undefined because we haven't synced yet.
    // We don't want to show the badge here because it's not an actual change,
    // we are just syncing for the first time.
    if (!this.syncedTabs) {
      return false;
    }

    // We loop through all windows to see if any window has currentURI "about:firefoxview" and
    // the window is visible because we don't want to show the notification badge in that case
    for (let window of lazy.BrowserWindowTracker.orderedWindows) {
      // if the url is "about:firefoxview" and the window visible we don't want to show the notification badge
      if (
        window.FirefoxViewHandler.tab?.selected &&
        !window.isFullyOccluded &&
        window.windowState !== window.STATE_MINIMIZED
      ) {
        return false;
      }
    }

    if (newTabs.length > this.syncedTabs.length) {
      return true;
    }
    for (let i = 0; i < newTabs.length; i++) {
      let newTab = newTabs[i];
      let oldTab = this.syncedTabs[i];

      if (newTab?.url !== oldTab?.url) {
        return true;
      }
    }
    return false;
  }

  shouldNotificationDotBeShowing() {
    return this.#currentlyShowing;
  }
})();
