"use strict";

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
   * have focus. A brower window is focused, then the portal is freed.
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

let singleRunTestCases = [
  /**
   * A portal is detected when there's no browser window,
   * then a browser window is opened, and the portal is logged into
   * and redirects to a different page. The portal tab should be added
   * and focused when the window is opened, and left open after login
   * since it redirected.
   */
  function* test_detectedWithNoBrowserWindow_Redirect() {
    yield portalDetected();
    let win = yield focusWindowAndWaitForPortalUI();
    let browser = win.gBrowser.selectedTab.linkedBrowser;
    let loadPromise =
      BrowserTestUtils.browserLoaded(browser, false, CANONICAL_URL_REDIRECTED);
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    yield loadPromise;
    yield freePortal(true);
    ensurePortalTab(win);
    ensureNoPortalNotification(win);
    yield closeWindowAndWaitForXulWindowVisible(win);
  },

  /**
   * Test the various expected behaviors of the "Show Login Page" button
   * in the captive portal notification. The button should be visible for
   * all tabs except the captive portal tab, and when clicked, should
   * ensure a captive portal tab is open and select it.
   */
  function* test_showLoginPageButton() {
    let win = yield openWindowAndWaitForFocus();
    yield portalDetected();
    let notification = ensurePortalNotification(win);
    testShowLoginPageButtonVisibility(notification, "visible");

    function testPortalTabSelectedAndButtonNotVisible() {
      is(win.gBrowser.selectedTab, tab, "The captive portal tab should be selected.");
      testShowLoginPageButtonVisibility(notification, "hidden");
    }

    let button = notification.querySelector("button.notification-button");
    function* clickButtonAndExpectNewPortalTab() {
      let p = BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
      button.click();
      let tab = yield p;
      is(win.gBrowser.selectedTab, tab, "The captive portal tab should be selected.");
      return tab;
    }

    // Simulate clicking the button. The portal tab should be opened and
    // selected and the button should hide.
    let tab = yield clickButtonAndExpectNewPortalTab();
    testPortalTabSelectedAndButtonNotVisible();

    // Close the tab. The button should become visible.
    yield BrowserTestUtils.removeTab(tab);
    ensureNoPortalTab(win);
    testShowLoginPageButtonVisibility(notification, "visible");

    // When the button is clicked, a new portal tab should be opened and
    // selected.
    tab = yield clickButtonAndExpectNewPortalTab();

    // Open another arbitrary tab. The button should become visible. When it's clicked,
    // the portal tab should be selected.
    let anotherTab = yield BrowserTestUtils.openNewForegroundTab(win.gBrowser);
    testShowLoginPageButtonVisibility(notification, "visible");
    button.click();
    is(win.gBrowser.selectedTab, tab, "The captive portal tab should be selected.");

    // Close the portal tab and select the arbitrary tab. The button should become
    // visible and when it's clicked, a new portal tab should be opened.
    yield BrowserTestUtils.removeTab(tab);
    win.gBrowser.selectedTab = anotherTab;
    testShowLoginPageButtonVisibility(notification, "visible");
    tab = yield clickButtonAndExpectNewPortalTab();

    yield BrowserTestUtils.removeTab(anotherTab);
    yield freePortal(true);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    yield closeWindowAndWaitForXulWindowVisible(win);
  },
];

for (let testcase of testCasesForBothSuccessAndAbort) {
  add_task(testcase.bind(null, true));
  add_task(testcase.bind(null, false));
}

for (let testcase of singleRunTestCases) {
  add_task(testcase);
}
