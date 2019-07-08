/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabUnloader"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * This module is responsible for detecting low-memory scenarios and unloading
 * tabs in response to them.
 */

var TabUnloader = {
  /**
   * Initialize low-memory detection and tab auto-unloading.
   */
  init() {
    if (Services.prefs.getBoolPref("browser.tabs.unloadOnLowMemory", true)) {
      Services.obs.addObserver(this, "memory-pressure", /* ownsWeak */ true);
    }
  },

  observe(subject, topic, data) {
    if (topic == "memory-pressure" && data != "heap-minimize") {
      unloadLeastRecentlyUsedTab();
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};

function unloadLeastRecentlyUsedTab() {
  let bgTabBrowsers = getSortedBackgroundTabBrowsers();

  for (let tb of bgTabBrowsers) {
    if (tb.browser.discardBrowser(tb.tab)) {
      return;
    }
  }
}

/* Sort tabs in the order we use for unloading, first non-pinned, non-audible
 * tabs in LRU order, then non-audible tabs in LRU order, then non-pinned
 * audible tabs in LRU order and finally pinned, audible tabs in LRU order. */
function sortTabs(a, b) {
  if (a.tab.soundPlaying != b.tab.soundPlaying) {
    return a.tab.soundPlaying - b.tab.soundPlaying;
  }

  if (a.tab.pinned != b.tab.pinned) {
    return a.tab.pinned - b.tab.pinned;
  }

  return a.tab.lastAccessed - b.tab.lastAccessed;
}

function getSortedBackgroundTabBrowsers() {
  let bgTabBrowsers = [];

  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    for (let tab of win.gBrowser.tabs) {
      if (!tab.selected && tab.linkedBrowser.isConnected) {
        bgTabBrowsers.push({ tab, browser: win.gBrowser });
      }
    }
  }

  return bgTabBrowsers.sort(sortTabs);
}
