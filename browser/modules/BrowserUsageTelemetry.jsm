/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["BrowserUsageTelemetry"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

// Observed topic names.
const WINDOWS_RESTORED_TOPIC = "sessionstore-windows-restored";
const TELEMETRY_SUBSESSIONSPLIT_TOPIC = "internal-telemetry-after-subsession-split";
const DOMWINDOW_OPENED_TOPIC = "domwindowopened";
const DOMWINDOW_CLOSED_TOPIC = "domwindowclosed";

// Probe names.
const MAX_TAB_COUNT_SCALAR_NAME = "browser.engagement.max_concurrent_tab_count";
const MAX_WINDOW_COUNT_SCALAR_NAME = "browser.engagement.max_concurrent_window_count";
const TAB_OPEN_EVENT_COUNT_SCALAR_NAME = "browser.engagement.tab_open_event_count";
const WINDOW_OPEN_EVENT_COUNT_SCALAR_NAME = "browser.engagement.window_open_event_count";

function getOpenTabsAndWinsCounts() {
  let tabCount = 0;
  let winCount = 0;

  let browserEnum = Services.wm.getEnumerator("navigator:browser");
  while (browserEnum.hasMoreElements()) {
    let win = browserEnum.getNext();
    winCount++;
    tabCount += win.gBrowser.tabs.length;
  }

  return { tabCount, winCount };
}

let BrowserUsageTelemetry = {
  init() {
    Services.obs.addObserver(this, WINDOWS_RESTORED_TOPIC, false);
  },

  /**
   * Handle subsession splits in the parent process.
   */
  afterSubsessionSplit() {
    // Scalars just got cleared due to a subsession split. We need to set the maximum
    // concurrent tab and window counts so that they reflect the correct value for the
    // new subsession.
    const counts = getOpenTabsAndWinsCounts();
    Services.telemetry.scalarSetMaximum(MAX_TAB_COUNT_SCALAR_NAME, counts.tabCount);
    Services.telemetry.scalarSetMaximum(MAX_WINDOW_COUNT_SCALAR_NAME, counts.winCount);
  },

  uninit() {
    Services.obs.removeObserver(this, DOMWINDOW_OPENED_TOPIC, false);
    Services.obs.removeObserver(this, DOMWINDOW_CLOSED_TOPIC, false);
    Services.obs.removeObserver(this, TELEMETRY_SUBSESSIONSPLIT_TOPIC, false);
    Services.obs.removeObserver(this, WINDOWS_RESTORED_TOPIC, false);
  },

  observe(subject, topic, data) {
    switch(topic) {
      case WINDOWS_RESTORED_TOPIC:
        this._setupAfterRestore();
        break;
      case DOMWINDOW_OPENED_TOPIC:
        this._onWindowOpen(subject);
        break;
      case DOMWINDOW_CLOSED_TOPIC:
        this._unregisterWindow(subject);
        break;
      case TELEMETRY_SUBSESSIONSPLIT_TOPIC:
        this.afterSubsessionSplit();
        break;
    }
  },

  handleEvent(event) {
    switch(event.type) {
      case "TabOpen":
        this._onTabOpen();
        break;
    }
  },

  /**
   * This gets called shortly after the SessionStore has finished restoring
   * windows and tabs. It counts the open tabs and adds listeners to all the
   * windows.
   */
  _setupAfterRestore() {
    // Make sure to catch new chrome windows and subsession splits.
    Services.obs.addObserver(this, DOMWINDOW_OPENED_TOPIC, false);
    Services.obs.addObserver(this, DOMWINDOW_CLOSED_TOPIC, false);
    Services.obs.addObserver(this, TELEMETRY_SUBSESSIONSPLIT_TOPIC, false);

    // Attach the tabopen handlers to the existing Windows.
    let browserEnum = Services.wm.getEnumerator("navigator:browser");
    while (browserEnum.hasMoreElements()) {
      this._registerWindow(browserEnum.getNext());
    }

    // Get the initial tab and windows max counts.
    const counts = getOpenTabsAndWinsCounts();
    Services.telemetry.scalarSetMaximum(MAX_TAB_COUNT_SCALAR_NAME, counts.tabCount);
    Services.telemetry.scalarSetMaximum(MAX_WINDOW_COUNT_SCALAR_NAME, counts.winCount);
  },

  /**
   * Adds listeners to a single chrome window.
   */
  _registerWindow(win) {
    win.addEventListener("TabOpen", this, true);
  },

  /**
   * Removes listeners from a single chrome window.
   */
  _unregisterWindow(win) {
    // Ignore non-browser windows.
    if (!(win instanceof Ci.nsIDOMWindow) ||
        win.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
      return;
    }

    win.removeEventListener("TabOpen", this, true);
  },

  /**
   * Updates the tab counts.
   * @param {Number} [newTabCount=0] The count of the opened tabs across all windows. This
   *        is computed manually if not provided.
   */
  _onTabOpen(tabCount = 0) {
    // Use the provided tab count if available. Otherwise, go on and compute it.
    tabCount = tabCount || getOpenTabsAndWinsCounts().tabCount;
    // Update the "tab opened" count and its maximum.
    Services.telemetry.scalarAdd(TAB_OPEN_EVENT_COUNT_SCALAR_NAME, 1);
    Services.telemetry.scalarSetMaximum(MAX_TAB_COUNT_SCALAR_NAME, tabCount);
  },

  /**
   * Tracks the window count and registers the listeners for the tab count.
   * @param{Object} win The window object.
   */
  _onWindowOpen(win) {
    // Make sure to have a |nsIDOMWindow|.
    if (!(win instanceof Ci.nsIDOMWindow)) {
      return;
    }

    let onLoad = () => {
      win.removeEventListener("load", onLoad, false);

      // Ignore non browser windows.
      if (win.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
        return;
      }

      this._registerWindow(win);
      // Track the window open event and check the maximum.
      const counts = getOpenTabsAndWinsCounts();
      Services.telemetry.scalarAdd(WINDOW_OPEN_EVENT_COUNT_SCALAR_NAME, 1);
      Services.telemetry.scalarSetMaximum(MAX_WINDOW_COUNT_SCALAR_NAME, counts.winCount);

      // We won't receive the "TabOpen" event for the first tab within a new window.
      // Account for that.
      this._onTabOpen(counts.tabCount);
    };
    win.addEventListener("load", onLoad, false);
  },
};
