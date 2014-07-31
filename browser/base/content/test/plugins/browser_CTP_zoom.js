/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;
const gHttpTestRoot = rootDir.replace("chrome://mochitests/content/",
                                      "http://127.0.0.1:8888/");

let gTestBrowser = null;

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(() => {
    FullZoom.reset();
    clearAllPluginPermissions();
    Services.prefs.clearUserPref("extensions.blocklist.suppressUI");
  });
  Services.prefs.setBoolPref("extensions.blocklist.suppressUI", true);

  Services.prefs.setBoolPref("plugins.click_to_play", true);
  setTestPluginEnabledState(Ci.nsIPluginTag.STATE_CLICKTOPLAY);

  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.contentWindow.location = gHttpTestRoot + "plugin_zoom.html"
}

function finishTest() {
  clearAllPluginPermissions();
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function pageLoad() {
  // Due to layout being async, "PluginBindAttached" may trigger later.
  // This wraps a function to force a layout flush, thus triggering it,
  // and schedules the function execution so they're definitely executed
  // afterwards.
  let doc = gTestBrowser.contentDocument;
  let elems = doc.getElementsByTagName('embed');
  if (elems.length < 1) {
    elems = doc.getElementsByTagName('object');
  }
  elems[0].clientTop;
  executeSoon(testOverlay);
}

let enlargeCount = 4;

// Enlarges the zoom level |enlargeCount| times and tests that the overlay is
// visible after each enlargement.
function testOverlay() {
  let plugin = gTestBrowser.contentDocument.getElementById("test");
  let doc = gTestBrowser.contentDocument;
  let overlay = doc.getAnonymousElementByAttribute(plugin, "anonid", "main");
  ok(overlay, "Overlay should exist");
  ok(overlay.classList.contains("visible"), "Overlay should be visible");

  if (enlargeCount > 0) {
    --enlargeCount;
    FullZoom.enlarge();
    gTestBrowser.contentWindow.location.reload();
  } else {
    FullZoom.reset();
    clearAllPluginPermissions();
    finishTest();
  }
}
