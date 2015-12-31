/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

const CHROMEUTILS_URL = "chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js";
var ChromeUtils = {};

Services.scriptloader.loadSubScript(CHROMEUTILS_URL, ChromeUtils);

/**
 * Tests that tabs from Private Browsing windows cannot be dragged
 * into non-private windows, and vice-versa.
 */
add_task(function* test_dragging_private_windows() {
  let normalWin = yield BrowserTestUtils.openNewBrowserWindow();
  let privateWin =
    yield BrowserTestUtils.openNewBrowserWindow({private: true});

  let normalTab =
    yield BrowserTestUtils.openNewForegroundTab(normalWin.gBrowser);
  let privateTab =
    yield BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser);

  let effect = ChromeUtils.synthesizeDrop(normalTab, privateTab,
    [[{type: TAB_DROP_TYPE, data: normalTab}]],
    null, normalWin, privateWin);
  is(effect, "none", "Should not be able to drag a normal tab to a private window");

  effect = ChromeUtils.synthesizeDrop(privateTab, normalTab,
    [[{type: TAB_DROP_TYPE, data: privateTab}]],
    null, privateWin, normalWin);
  is(effect, "none", "Should not be able to drag a private tab to a normal window");

  normalWin.gBrowser.swapBrowsersAndCloseOther(normalTab, privateTab);
  is(normalWin.gBrowser.tabs.length, 2,
     "Prevent moving a normal tab to a private tabbrowser");
  is(privateWin.gBrowser.tabs.length, 2,
     "Prevent accepting a normal tab in a private tabbrowser");

  privateWin.gBrowser.swapBrowsersAndCloseOther(privateTab, normalTab);
  is(privateWin.gBrowser.tabs.length, 2,
     "Prevent moving a private tab to a normal tabbrowser");
  is(normalWin.gBrowser.tabs.length, 2,
     "Prevent accepting a private tab in a normal tabbrowser");

  yield BrowserTestUtils.closeWindow(normalWin);
  yield BrowserTestUtils.closeWindow(privateWin);
});

/**
 * Tests that tabs from e10s windows cannot be dragged into non-e10s
 * windows, and vice-versa.
 */
add_task(function* test_dragging_e10s_windows() {
  if (!gMultiProcessBrowser) {
    return;
  }

  let remoteWin = yield BrowserTestUtils.openNewBrowserWindow({remote: true});
  let nonRemoteWin = yield BrowserTestUtils.openNewBrowserWindow({remote: false});

  let remoteTab =
    yield BrowserTestUtils.openNewForegroundTab(remoteWin.gBrowser);
  let nonRemoteTab =
    yield BrowserTestUtils.openNewForegroundTab(nonRemoteWin.gBrowser);

  let effect = ChromeUtils.synthesizeDrop(remoteTab, nonRemoteTab,
    [[{type: TAB_DROP_TYPE, data: remoteTab}]],
    null, remoteWin, nonRemoteWin);
  is(effect, "none", "Should not be able to drag a remote tab to a non-e10s window");

  effect = ChromeUtils.synthesizeDrop(nonRemoteTab, remoteTab,
    [[{type: TAB_DROP_TYPE, data: nonRemoteTab}]],
    null, nonRemoteWin, remoteWin);
  is(effect, "none", "Should not be able to drag a non-remote tab to an e10s window");

  remoteWin.gBrowser.swapBrowsersAndCloseOther(remoteTab, nonRemoteTab);
  is(remoteWin.gBrowser.tabs.length, 2,
     "Prevent moving a normal tab to a private tabbrowser");
  is(nonRemoteWin.gBrowser.tabs.length, 2,
     "Prevent accepting a normal tab in a private tabbrowser");

  nonRemoteWin.gBrowser.swapBrowsersAndCloseOther(nonRemoteTab, remoteTab);
  is(nonRemoteWin.gBrowser.tabs.length, 2,
     "Prevent moving a private tab to a normal tabbrowser");
  is(remoteWin.gBrowser.tabs.length, 2,
     "Prevent accepting a private tab in a normal tabbrowser");

  yield BrowserTestUtils.closeWindow(remoteWin);
  yield BrowserTestUtils.closeWindow(nonRemoteWin);
});

/**
 * Tests that remoteness-blacklisted tabs from e10s windows can
 * be dragged between e10s windows.
 */
add_task(function* test_dragging_blacklisted() {
  if (!gMultiProcessBrowser) {
    return;
  }

  let remoteWin1 = yield BrowserTestUtils.openNewBrowserWindow({remote: true});
  remoteWin1.gBrowser.myID = "remoteWin1";
  let remoteWin2 = yield BrowserTestUtils.openNewBrowserWindow({remote: true});
  remoteWin2.gBrowser.myID = "remoteWin2";

  // Anything under chrome://mochitests/content/ will be blacklisted, and
  // open in the parent process.
  const BLACKLISTED_URL = getRootDirectory(gTestPath) +
                          "browser_tab_drag_drop_perwindow.js";
  let blacklistedTab =
    yield BrowserTestUtils.openNewForegroundTab(remoteWin1.gBrowser,
                                                BLACKLISTED_URL);

  ok(blacklistedTab.linkedBrowser, "Newly created tab should have a browser.");

  ok(!blacklistedTab.linkedBrowser.isRemoteBrowser,
     `Expected a non-remote browser for URL: ${BLACKLISTED_URL}`);

  let otherTab =
    yield BrowserTestUtils.openNewForegroundTab(remoteWin2.gBrowser);

  let effect = ChromeUtils.synthesizeDrop(blacklistedTab, otherTab,
    [[{type: TAB_DROP_TYPE, data: blacklistedTab}]],
    null, remoteWin1, remoteWin2);
  is(effect, "move", "Should be able to drag the blacklisted tab.");

  // The synthesized drop should also do the work of swapping the
  // browsers, so no need to call swapBrowsersAndCloseOther manually.

  is(remoteWin1.gBrowser.tabs.length, 1,
     "Should have moved the blacklisted tab out of this window.");
  is(remoteWin2.gBrowser.tabs.length, 3,
     "Should have inserted the blacklisted tab into the other window.");

  // The currently selected tab in the second window should be the
  // one we just dragged in.
  let draggedBrowser = remoteWin2.gBrowser.selectedBrowser;
  ok(!draggedBrowser.isRemoteBrowser,
     "The browser we just dragged in should not be remote.");

  is(draggedBrowser.currentURI.spec, BLACKLISTED_URL,
     `Expected the URL of the dragged in tab to be ${BLACKLISTED_URL}`);

  yield BrowserTestUtils.closeWindow(remoteWin1);
  yield BrowserTestUtils.closeWindow(remoteWin2);
});
