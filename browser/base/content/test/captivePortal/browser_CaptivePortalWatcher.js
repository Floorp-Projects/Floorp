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
  function* test_detectedWithNoBrowserWindow_Open(aSuccess) {
    yield portalDetected();
    let win = yield focusWindowAndWaitForPortalUI();
    yield freePortal(aSuccess);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    yield closeWindowAndWaitForXulWindowVisible(win);
  },

  /**
   * A portal is detected when multiple browser windows are open but none
   * have focus. A browser window is focused, then the portal is freed.
   * The portal tab should be added and focused when the window is
   * focused, and closed automatically when the success event is fired.
   * The captive portal notification should be shown in all windows upon
   * detection, and closed automatically when the success event is fired.
   */
  function* test_detectedWithNoBrowserWindow_Focused(aSuccess) {
    let win1 = yield openWindowAndWaitForFocus();
    let win2 = yield openWindowAndWaitForFocus();
    // Defocus both windows.
    yield SimpleTest.promiseFocus(window);

    yield portalDetected();

    // Notification should be shown in both windows.
    ensurePortalNotification(win1);
    ensureNoPortalTab(win1);
    ensurePortalNotification(win2);
    ensureNoPortalTab(win2);

    yield focusWindowAndWaitForPortalUI(false, win2);

    yield freePortal(aSuccess);

    ensureNoPortalNotification(win1);
    ensureNoPortalTab(win2);
    ensureNoPortalNotification(win2);

    yield closeWindowAndWaitForXulWindowVisible(win2);
    // No need to wait for xul-window-visible: after win2 is closed, focus
    // is restored to the default window and win1 remains in the background.
    yield BrowserTestUtils.closeWindow(win1);
  },

  /**
   * A portal is detected when there's no browser window, then a browser
   * window is opened, then the portal is freed.
   * The recheck triggered when the browser window is opened takes a
   * long time. No portal tab should be added.
   * The captive portal notification should be shown when the window is
   * opened, and closed automatically when the success event is fired.
   */
  function* test_detectedWithNoBrowserWindow_LongRecheck(aSuccess) {
    yield portalDetected();
    let win = yield focusWindowAndWaitForPortalUI(true);
    yield freePortal(aSuccess);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    yield closeWindowAndWaitForXulWindowVisible(win);
  },

  /**
   * A portal is detected when there's no browser window, and the
   * portal is freed before a browser window is opened. No portal
   * UI should be shown when a browser window is opened.
   */
  function* test_detectedWithNoBrowserWindow_GoneBeforeOpen(aSuccess) {
    yield portalDetected();
    yield freePortal(aSuccess);
    let win = yield openWindowAndWaitForFocus();
    // Wait for a while to make sure no UI is shown.
    yield new Promise(resolve => {
      setTimeout(resolve, 1000);
    });
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    yield closeWindowAndWaitForXulWindowVisible(win);
  },

  /**
   * A portal is detected when a browser window has focus. No portal tab should
   * be opened. A notification bar should be displayed in all browser windows.
   */
  function* test_detectedWithFocus(aSuccess) {
    let win1 = yield openWindowAndWaitForFocus();
    let win2 = yield openWindowAndWaitForFocus();
    yield portalDetected();
    ensureNoPortalTab(win1);
    ensureNoPortalTab(win2);
    ensurePortalNotification(win1);
    ensurePortalNotification(win2);
    yield freePortal(aSuccess);
    ensureNoPortalNotification(win1);
    ensureNoPortalNotification(win2);
    yield closeWindowAndWaitForXulWindowVisible(win2);
    yield closeWindowAndWaitForXulWindowVisible(win1);
  },
];

for (let testcase of testCasesForBothSuccessAndAbort) {
  add_task(testcase.bind(null, true));
  add_task(testcase.bind(null, false));
}
