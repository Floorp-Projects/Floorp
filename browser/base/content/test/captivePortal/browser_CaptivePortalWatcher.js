/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

// Bug 1318389 - This test does a lot of window and tab manipulation,
//               causing it to take a long time on debug.
requestLongerTimeout(2);

add_task(setupPrefsAndRecentWindowBehavior);

// Each of the test cases below is run twice: once for login-success and once
// for login-abort (aSuccess set to true and false respectively).
let testCasesForBothSuccessAndAbort = [
  /**
   * A portal is detected when there's no browser window, then a browser
   * window is opened, then the portal is freed.
   * The portal tab should be added and focused when the window is
   * opened, and closed automatically when the success event is fired.
   * The captive portal notification should be shown when the window is
   * opened, and closed automatically when the success event is fired.
   */
  async function test_detectedWithNoBrowserWindow_Open(aSuccess) {
    await portalDetected();
    let win = await focusWindowAndWaitForPortalUI();
    await freePortal(aSuccess);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    await closeWindowAndWaitForWindowActivate(win);
  },

  /**
   * A portal is detected when multiple browser windows are open but none
   * have focus. A browser window is focused, then the portal is freed.
   * The portal tab should be added and focused when the window is
   * focused, and closed automatically when the success event is fired.
   * The captive portal notification should be shown in all windows upon
   * detection, and closed automatically when the success event is fired.
   */
  async function test_detectedWithNoBrowserWindow_Focused(aSuccess) {
    let win1 = await openWindowAndWaitForFocus();
    let win2 = await openWindowAndWaitForFocus();
    // Defocus both windows.
    await SimpleTest.promiseFocus(window);

    await portalDetected();

    // Notification should be shown in both windows.
    await ensurePortalNotification(win1);
    ensureNoPortalTab(win1);
    await ensurePortalNotification(win2);
    ensureNoPortalTab(win2);

    await focusWindowAndWaitForPortalUI(false, win2);

    await freePortal(aSuccess);

    ensureNoPortalNotification(win1);
    ensureNoPortalTab(win2);
    ensureNoPortalNotification(win2);

    await closeWindowAndWaitForWindowActivate(win2);
    // No need to wait for xul-window-visible: after win2 is closed, focus
    // is restored to the default window and win1 remains in the background.
    await BrowserTestUtils.closeWindow(win1);
  },

  /**
   * A portal is detected when there's no browser window, then a browser
   * window is opened, then the portal is freed.
   * The recheck triggered when the browser window is opened takes a
   * long time. No portal tab should be added.
   * The captive portal notification should be shown when the window is
   * opened, and closed automatically when the success event is fired.
   */
  async function test_detectedWithNoBrowserWindow_LongRecheck(aSuccess) {
    await portalDetected();
    let win = await focusWindowAndWaitForPortalUI(true);
    await freePortal(aSuccess);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    await closeWindowAndWaitForWindowActivate(win);
  },

  /**
   * A portal is detected when there's no browser window, and the
   * portal is freed before a browser window is opened. No portal
   * UI should be shown when a browser window is opened.
   */
  async function test_detectedWithNoBrowserWindow_GoneBeforeOpen(aSuccess) {
    await portalDetected();
    await freePortal(aSuccess);
    let win = await openWindowAndWaitForFocus();
    // Wait for a while to make sure no UI is shown.
    await new Promise(resolve => {
      setTimeout(resolve, 1000);
    });
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    await closeWindowAndWaitForWindowActivate(win);
  },

  /**
   * A portal is detected when a browser window has focus. No portal tab should
   * be opened. A notification bar should be displayed in all browser windows.
   */
  async function test_detectedWithFocus(aSuccess) {
    let win1 = await openWindowAndWaitForFocus();
    let win2 = await openWindowAndWaitForFocus();
    await portalDetected();
    ensureNoPortalTab(win1);
    ensureNoPortalTab(win2);
    await ensurePortalNotification(win1);
    await ensurePortalNotification(win2);
    await freePortal(aSuccess);
    ensureNoPortalNotification(win1);
    ensureNoPortalNotification(win2);
    await BrowserTestUtils.closeWindow(win2);
    await BrowserTestUtils.closeWindow(win1);
    await waitForBrowserWindowActive(window);
  },
];

for (let testcase of testCasesForBothSuccessAndAbort) {
  add_task(testcase.bind(null, true));
  add_task(testcase.bind(null, false));
}
