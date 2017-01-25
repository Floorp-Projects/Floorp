"use strict";

Components.utils.import("resource:///modules/RecentWindow.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CaptivePortalWatcher",
  "resource:///modules/CaptivePortalWatcher.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cps",
                                   "@mozilla.org/network/captive-portal-service;1",
                                   "nsICaptivePortalService");

const CANONICAL_CONTENT = "success";
const CANONICAL_URL = "data:text/plain;charset=utf-8," + CANONICAL_CONTENT;
const CANONICAL_URL_REDIRECTED = "data:text/plain;charset=utf-8,redirected";
const PORTAL_NOTIFICATION_VALUE = "captive-portal-detected";

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["captivedetect.canonicalURL", CANONICAL_URL],
          ["captivedetect.canonicalContent", CANONICAL_CONTENT]],
  });
  // We need to test behavior when a portal is detected when there is no browser
  // window, but we can't close the default window opened by the test harness.
  // Instead, we deactivate CaptivePortalWatcher in the default window and
  // exclude it from RecentWindow.getMostRecentBrowserWindow in an attempt to
  // mask its presence.
  window.CaptivePortalWatcher.uninit();
  RecentWindow._getMostRecentBrowserWindowCopy = RecentWindow.getMostRecentBrowserWindow;
  let defaultWindow = window;
  RecentWindow.getMostRecentBrowserWindow = () => {
    let win = RecentWindow._getMostRecentBrowserWindowCopy();
    if (win == defaultWindow) {
      return null;
    }
    return win;
  };
});

function* portalDetected() {
  Services.obs.notifyObservers(null, "captive-portal-login", null);
  yield BrowserTestUtils.waitForCondition(() => {
    return cps.state == cps.LOCKED_PORTAL;
  }, "Waiting for Captive Portal Service to update state after portal detected.");
}

function* freePortal(aSuccess) {
  Services.obs.notifyObservers(null,
    "captive-portal-login-" + (aSuccess ? "success" : "abort"), null);
  yield BrowserTestUtils.waitForCondition(() => {
    return cps.state != cps.LOCKED_PORTAL;
  }, "Waiting for Captive Portal Service to update state after portal freed.");
}

// If a window is provided, it will be focused. Otherwise, a new window
// will be opened and focused.
function* focusWindowAndWaitForPortalUI(aLongRecheck, win) {
  // CaptivePortalWatcher triggers a recheck when a window gains focus. If
  // the time taken for the check to complete is under PORTAL_RECHECK_DELAY_MS,
  // a tab with the login page is opened and selected. If it took longer,
  // no tab is opened. It's not reliable to time things in an async test,
  // so use a delay threshold of -1 to simulate a long recheck (so that any
  // amount of time is considered excessive), and a very large threshold to
  // simulate a short recheck.
  Preferences.set("captivedetect.portalRecheckDelayMS", aLongRecheck ? -1 : 1000000);

  if (!win) {
    win = yield BrowserTestUtils.openNewBrowserWindow();
  }
  yield SimpleTest.promiseFocus(win);

  // After a new window is opened, CaptivePortalWatcher asks for a recheck, and
  // waits for it to complete. We need to manually tell it a recheck completed.
  yield BrowserTestUtils.waitForCondition(() => {
    return win.CaptivePortalWatcher._waitingForRecheck;
  }, "Waiting for CaptivePortalWatcher to trigger a recheck.");
  Services.obs.notifyObservers(null, "captive-portal-check-complete", null);

  let notification = ensurePortalNotification(win);

  if (aLongRecheck) {
    ensureNoPortalTab(win);
    testShowLoginPageButtonVisibility(notification, "visible");
    return win;
  }

  let tab = win.gBrowser.tabs[1];
  if (tab.linkedBrowser.currentURI.spec != CANONICAL_URL) {
    // The tab should load the canonical URL, wait for it.
    yield BrowserTestUtils.waitForLocationChange(win.gBrowser, CANONICAL_URL);
  }
  is(win.gBrowser.selectedTab, tab,
    "The captive portal tab should be open and selected in the new window.");
  testShowLoginPageButtonVisibility(notification, "hidden");
  return win;
}

function ensurePortalTab(win) {
  // For the tests that call this function, it's enough to ensure there
  // are two tabs in the window - the default tab and the portal tab.
  is(win.gBrowser.tabs.length, 2,
    "There should be a captive portal tab in the window.");
}

function ensurePortalNotification(win) {
  let notificationBox =
    win.document.getElementById("high-priority-global-notificationbox");
  let notification = notificationBox.getNotificationWithValue(PORTAL_NOTIFICATION_VALUE)
  isnot(notification, null,
    "There should be a captive portal notification in the window.");
  return notification;
}

// Helper to test whether the "Show Login Page" is visible in the captive portal
// notification (it should be hidden when the portal tab is selected).
function testShowLoginPageButtonVisibility(notification, visibility) {
  let showLoginPageButton = notification.querySelector("button.notification-button");
  // If the visibility property was never changed from default, it will be
  // an empty string, so we pretend it's "visible" (effectively the same).
  is(showLoginPageButton.style.visibility || "visible", visibility,
    "The \"Show Login Page\" button should be " + visibility + ".");
}

function ensureNoPortalTab(win) {
  is(win.gBrowser.tabs.length, 1,
    "There should be no captive portal tab in the window.");
}

function ensureNoPortalNotification(win) {
  let notificationBox =
    win.document.getElementById("high-priority-global-notificationbox");
  is(notificationBox.getNotificationWithValue(PORTAL_NOTIFICATION_VALUE), null,
    "There should be no captive portal notification in the window.");
}

/**
 * Some tests open a new window and close it later. When the window is closed,
 * the original window opened by mochitest gains focus, generating a
 * xul-window-visible notification. If the next test also opens a new window
 * before this notification has a chance to fire, CaptivePortalWatcher picks
 * up the first one instead of the one from the new window. To avoid this
 * unfortunate intermittent timing issue, we wait for the notification from
 * the original window every time we close a window that we opened.
 */
function waitForXulWindowVisible() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observe() {
      Services.obs.removeObserver(observe, "xul-window-visible");
      resolve();
    }, "xul-window-visible", false);
  });
}

function* closeWindowAndWaitForXulWindowVisible(win) {
  let p = waitForXulWindowVisible();
  yield BrowserTestUtils.closeWindow(win);
  yield p;
}

/**
 * BrowserTestUtils.openNewBrowserWindow() does not guarantee the newly
 * opened window has received focus when the promise resolves, so we
 * have to manually wait every time.
 */
function* openWindowAndWaitForFocus() {
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  yield SimpleTest.promiseFocus(win);
  return win;
}

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

add_task(function* cleanUp() {
  RecentWindow.getMostRecentBrowserWindow = RecentWindow._getMostRecentBrowserWindowCopy;
  delete RecentWindow._getMostRecentBrowserWindowCopy;
  window.CaptivePortalWatcher.init();
});
