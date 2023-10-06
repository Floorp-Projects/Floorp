/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

/**
 * Tests that tabs from Private Browsing windows cannot be dragged
 * into non-private windows, and vice-versa.
 */
add_task(async function test_dragging_private_windows() {
  let normalWin = await BrowserTestUtils.openNewBrowserWindow();
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let normalTab = await BrowserTestUtils.openNewForegroundTab(
    normalWin.gBrowser
  );
  let privateTab = await BrowserTestUtils.openNewForegroundTab(
    privateWin.gBrowser
  );

  let effect = EventUtils.synthesizeDrop(
    normalTab,
    privateTab,
    [[{ type: TAB_DROP_TYPE, data: normalTab }]],
    null,
    normalWin,
    privateWin
  );
  is(
    effect,
    "none",
    "Should not be able to drag a normal tab to a private window"
  );

  effect = EventUtils.synthesizeDrop(
    privateTab,
    normalTab,
    [[{ type: TAB_DROP_TYPE, data: privateTab }]],
    null,
    privateWin,
    normalWin
  );
  is(
    effect,
    "none",
    "Should not be able to drag a private tab to a normal window"
  );

  normalWin.gBrowser.swapBrowsersAndCloseOther(normalTab, privateTab);
  is(
    normalWin.gBrowser.tabs.length,
    2,
    "Prevent moving a normal tab to a private tabbrowser"
  );
  is(
    privateWin.gBrowser.tabs.length,
    2,
    "Prevent accepting a normal tab in a private tabbrowser"
  );

  privateWin.gBrowser.swapBrowsersAndCloseOther(privateTab, normalTab);
  is(
    privateWin.gBrowser.tabs.length,
    2,
    "Prevent moving a private tab to a normal tabbrowser"
  );
  is(
    normalWin.gBrowser.tabs.length,
    2,
    "Prevent accepting a private tab in a normal tabbrowser"
  );

  await BrowserTestUtils.closeWindow(normalWin);
  await BrowserTestUtils.closeWindow(privateWin);
});

/**
 * Tests that tabs from e10s windows cannot be dragged into non-e10s
 * windows, and vice-versa.
 */
add_task(async function test_dragging_e10s_windows() {
  if (!gMultiProcessBrowser) {
    return;
  }

  let remoteWin = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let nonRemoteWin = await BrowserTestUtils.openNewBrowserWindow({
    remote: false,
    fission: false,
  });

  let remoteTab = await BrowserTestUtils.openNewForegroundTab(
    remoteWin.gBrowser
  );
  let nonRemoteTab = await BrowserTestUtils.openNewForegroundTab(
    nonRemoteWin.gBrowser
  );

  let effect = EventUtils.synthesizeDrop(
    remoteTab,
    nonRemoteTab,
    [[{ type: TAB_DROP_TYPE, data: remoteTab }]],
    null,
    remoteWin,
    nonRemoteWin
  );
  is(
    effect,
    "none",
    "Should not be able to drag a remote tab to a non-e10s window"
  );

  effect = EventUtils.synthesizeDrop(
    nonRemoteTab,
    remoteTab,
    [[{ type: TAB_DROP_TYPE, data: nonRemoteTab }]],
    null,
    nonRemoteWin,
    remoteWin
  );
  is(
    effect,
    "none",
    "Should not be able to drag a non-remote tab to an e10s window"
  );

  remoteWin.gBrowser.swapBrowsersAndCloseOther(remoteTab, nonRemoteTab);
  is(
    remoteWin.gBrowser.tabs.length,
    2,
    "Prevent moving a normal tab to a private tabbrowser"
  );
  is(
    nonRemoteWin.gBrowser.tabs.length,
    2,
    "Prevent accepting a normal tab in a private tabbrowser"
  );

  nonRemoteWin.gBrowser.swapBrowsersAndCloseOther(nonRemoteTab, remoteTab);
  is(
    nonRemoteWin.gBrowser.tabs.length,
    2,
    "Prevent moving a private tab to a normal tabbrowser"
  );
  is(
    remoteWin.gBrowser.tabs.length,
    2,
    "Prevent accepting a private tab in a normal tabbrowser"
  );

  await BrowserTestUtils.closeWindow(remoteWin);
  await BrowserTestUtils.closeWindow(nonRemoteWin);
});

/**
 * Tests that tabs from fission windows cannot be dragged into non-fission
 * windows, and vice-versa.
 */
add_task(async function test_dragging_fission_windows() {
  let fissionWin = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
    fission: true,
  });
  let nonFissionWin = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
    fission: false,
  });

  let fissionTab = await BrowserTestUtils.openNewForegroundTab(
    fissionWin.gBrowser
  );
  let nonFissionTab = await BrowserTestUtils.openNewForegroundTab(
    nonFissionWin.gBrowser
  );

  let effect = EventUtils.synthesizeDrop(
    fissionTab,
    nonFissionTab,
    [[{ type: TAB_DROP_TYPE, data: fissionTab }]],
    null,
    fissionWin,
    nonFissionWin
  );
  is(
    effect,
    "none",
    "Should not be able to drag a fission tab to a non-fission window"
  );

  effect = EventUtils.synthesizeDrop(
    nonFissionTab,
    fissionTab,
    [[{ type: TAB_DROP_TYPE, data: nonFissionTab }]],
    null,
    nonFissionWin,
    fissionWin
  );
  is(
    effect,
    "none",
    "Should not be able to drag a non-fission tab to an fission window"
  );

  let swapOk = fissionWin.gBrowser.swapBrowsersAndCloseOther(
    fissionTab,
    nonFissionTab
  );
  is(
    swapOk,
    false,
    "Returns false swapping fission tab to a non-fission tabbrowser"
  );
  is(
    fissionWin.gBrowser.tabs.length,
    2,
    "Prevent moving a fission tab to a non-fission tabbrowser"
  );
  is(
    nonFissionWin.gBrowser.tabs.length,
    2,
    "Prevent accepting a fission tab in a non-fission tabbrowser"
  );

  swapOk = nonFissionWin.gBrowser.swapBrowsersAndCloseOther(
    nonFissionTab,
    fissionTab
  );
  is(
    swapOk,
    false,
    "Returns false swapping non-fission tab to a fission tabbrowser"
  );
  is(
    nonFissionWin.gBrowser.tabs.length,
    2,
    "Prevent moving a non-fission tab to a fission tabbrowser"
  );
  is(
    fissionWin.gBrowser.tabs.length,
    2,
    "Prevent accepting a non-fission tab in a fission tabbrowser"
  );

  await BrowserTestUtils.closeWindow(fissionWin);
  await BrowserTestUtils.closeWindow(nonFissionWin);
});

/**
 * Tests that remoteness-blacklisted tabs from e10s windows can
 * be dragged between e10s windows.
 */
add_task(async function test_dragging_blacklisted() {
  if (!gMultiProcessBrowser) {
    return;
  }

  let remoteWin1 = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
  });
  remoteWin1.gBrowser.myID = "remoteWin1";
  let remoteWin2 = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
  });
  remoteWin2.gBrowser.myID = "remoteWin2";

  // Anything under chrome://mochitests/content/ will be blacklisted, and
  // open in the parent process.
  const BLACKLISTED_URL =
    getRootDirectory(gTestPath) + "browser_tab_drag_drop_perwindow.js";
  let blacklistedTab = await BrowserTestUtils.openNewForegroundTab(
    remoteWin1.gBrowser,
    BLACKLISTED_URL
  );

  ok(blacklistedTab.linkedBrowser, "Newly created tab should have a browser.");

  ok(
    !blacklistedTab.linkedBrowser.isRemoteBrowser,
    `Expected a non-remote browser for URL: ${BLACKLISTED_URL}`
  );

  let otherTab = await BrowserTestUtils.openNewForegroundTab(
    remoteWin2.gBrowser
  );

  let effect = EventUtils.synthesizeDrop(
    blacklistedTab,
    otherTab,
    [[{ type: TAB_DROP_TYPE, data: blacklistedTab }]],
    null,
    remoteWin1,
    remoteWin2
  );
  is(effect, "move", "Should be able to drag the blacklisted tab.");

  // The synthesized drop should also do the work of swapping the
  // browsers, so no need to call swapBrowsersAndCloseOther manually.

  is(
    remoteWin1.gBrowser.tabs.length,
    1,
    "Should have moved the blacklisted tab out of this window."
  );
  is(
    remoteWin2.gBrowser.tabs.length,
    3,
    "Should have inserted the blacklisted tab into the other window."
  );

  // The currently selected tab in the second window should be the
  // one we just dragged in.
  let draggedBrowser = remoteWin2.gBrowser.selectedBrowser;
  ok(
    !draggedBrowser.isRemoteBrowser,
    "The browser we just dragged in should not be remote."
  );

  is(
    draggedBrowser.currentURI.spec,
    BLACKLISTED_URL,
    `Expected the URL of the dragged in tab to be ${BLACKLISTED_URL}`
  );

  await BrowserTestUtils.closeWindow(remoteWin1);
  await BrowserTestUtils.closeWindow(remoteWin2);
});

/**
 * Tests that tabs dragged between windows dispatch TabOpen and TabClose
 * events with the appropriate adoption details.
 */
add_task(async function test_dragging_adoption_events() {
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(win1.gBrowser);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(win2.gBrowser);

  let awaitCloseEvent = BrowserTestUtils.waitForEvent(tab1, "TabClose");
  let awaitOpenEvent = BrowserTestUtils.waitForEvent(win2, "TabOpen");

  let effect = EventUtils.synthesizeDrop(
    tab1,
    tab2,
    [[{ type: TAB_DROP_TYPE, data: tab1 }]],
    null,
    win1,
    win2
  );
  is(effect, "move", "Tab should be moved from win1 to win2.");

  let closeEvent = await awaitCloseEvent;
  let openEvent = await awaitOpenEvent;

  is(openEvent.detail.adoptedTab, tab1, "New tab adopted old tab");
  is(
    closeEvent.detail.adoptedBy,
    openEvent.target,
    "Old tab adopted by new tab"
  );

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});

/**
 * Tests that per-site zoom settings remain active after a tab is
 * dragged between windows.
 */
add_task(async function test_dragging_zoom_handling() {
  const ZOOM_FACTOR = 1.62;

  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(win1.gBrowser);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    win2.gBrowser,
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com/"
  );

  win2.FullZoom.setZoom(ZOOM_FACTOR);
  is(
    ZoomManager.getZoomForBrowser(tab2.linkedBrowser),
    ZOOM_FACTOR,
    "Original tab should have correct zoom factor"
  );

  let effect = EventUtils.synthesizeDrop(
    tab2,
    tab1,
    [[{ type: TAB_DROP_TYPE, data: tab2 }]],
    null,
    win2,
    win1
  );
  is(effect, "move", "Tab should be moved from win2 to win1.");

  // Delay slightly to make sure we've finished executing any promise
  // chains in the zoom code.
  await new Promise(resolve => setTimeout(resolve, 0));

  is(
    ZoomManager.getZoomForBrowser(win1.gBrowser.selectedBrowser),
    ZOOM_FACTOR,
    "Dragged tab should have correct zoom factor"
  );

  win1.FullZoom.reset();

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});
