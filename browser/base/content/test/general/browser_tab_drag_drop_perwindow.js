/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

const EVENTUTILS_URL = "chrome://mochikit/content/tests/SimpleTest/EventUtils.js";
var EventUtils = {};

Services.scriptloader.loadSubScript(EVENTUTILS_URL, EventUtils);

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

  let effect = EventUtils.synthesizeDrop(normalTab, privateTab,
    [[{type: TAB_DROP_TYPE, data: normalTab}]],
    null, normalWin, privateWin);
  is(effect, "none", "Should not be able to drag a normal tab to a private window");

  effect = EventUtils.synthesizeDrop(privateTab, normalTab,
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

  let effect = EventUtils.synthesizeDrop(remoteTab, nonRemoteTab,
    [[{type: TAB_DROP_TYPE, data: remoteTab}]],
    null, remoteWin, nonRemoteWin);
  is(effect, "none", "Should not be able to drag a remote tab to a non-e10s window");

  effect = EventUtils.synthesizeDrop(nonRemoteTab, remoteTab,
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

  let effect = EventUtils.synthesizeDrop(blacklistedTab, otherTab,
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


/**
 * Tests that tabs dragged between windows dispatch TabOpen and TabClose
 * events with the appropriate adoption details.
 */
add_task(function* test_dragging_adoption_events() {
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();

  let tab1 = yield BrowserTestUtils.openNewForegroundTab(win1.gBrowser);
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(win2.gBrowser);

  let awaitCloseEvent = BrowserTestUtils.waitForEvent(tab1, "TabClose");
  let awaitOpenEvent = BrowserTestUtils.waitForEvent(win2, "TabOpen");

  let effect = EventUtils.synthesizeDrop(tab1, tab2,
    [[{type: TAB_DROP_TYPE, data: tab1}]],
    null, win1, win2);
  is(effect, "move", "Tab should be moved from win1 to win2.");

  let closeEvent = yield awaitCloseEvent;
  let openEvent = yield awaitOpenEvent;

  is(openEvent.detail.adoptedTab, tab1, "New tab adopted old tab");
  is(closeEvent.detail.adoptedBy, openEvent.target, "Old tab adopted by new tab");

  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);
});


/**
 * Tests that per-site zoom settings remain active after a tab is
 * dragged between windows.
 */
add_task(function* test_dragging_zoom_handling() {
  const ZOOM_FACTOR = 1.62;

  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();

  let tab1 = yield BrowserTestUtils.openNewForegroundTab(win1.gBrowser);
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(win2.gBrowser,
                                                         "http://example.com/");

  win2.FullZoom.setZoom(ZOOM_FACTOR);
  FullZoomHelper.zoomTest(tab2, ZOOM_FACTOR,
                          "Original tab should have correct zoom factor");

  let effect = EventUtils.synthesizeDrop(tab2, tab1,
    [[{type: TAB_DROP_TYPE, data: tab2}]],
    null, win2, win1);
  is(effect, "move", "Tab should be moved from win2 to win1.");

  // Delay slightly to make sure we've finished executing any promise
  // chains in the zoom code.
  yield new Promise(resolve => setTimeout(resolve, 0));

  FullZoomHelper.zoomTest(win1.gBrowser.selectedTab, ZOOM_FACTOR,
                          "Dragged tab should have correct zoom factor");

  win1.FullZoom.reset();

  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);
});
