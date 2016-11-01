"use strict";

Components.utils.import("resource:///modules/RecentWindow.jsm");

const CANONICAL_CONTENT = "success";
const CANONICAL_URL = "data:text/plain;charset=utf-8," + CANONICAL_CONTENT;
const CANONICAL_URL_REDIRECTED = "data:text/plain;charset=utf-8,redirected";
const PORTAL_NOTIFICATION_VALUE = "captive-portal-detected";

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["captivedetect.canonicalURL", CANONICAL_URL],
          ["captivedetect.canonicalContent", CANONICAL_CONTENT]],
  });
});

/**
 * We can't close the original window opened by mochitest without failing, so
 * override RecentWindow.getMostRecentBrowserWindow to make CaptivePortalWatcher
 * think there's no window.
 */
function* portalDetectedNoBrowserWindow() {
  let getMostRecentBrowserWindow = RecentWindow.getMostRecentBrowserWindow;
  RecentWindow.getMostRecentBrowserWindow = () => {};
  Services.obs.notifyObservers(null, "captive-portal-login", null);
  RecentWindow.getMostRecentBrowserWindow = getMostRecentBrowserWindow;
}

function* openWindowAndWaitForPortalTabAndNotification() {
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  // Thanks to things being async, at this point we now have a new browser window
  // but the portal notification and tab may or may not have opened. So first we
  // check if there's already a portal notification, and if not, wait.
  let notification = win.document.getElementById("high-priority-global-notificationbox")
                        .getNotificationWithValue(PORTAL_NOTIFICATION_VALUE);
  if (!notification) {
    notification =
      yield BrowserTestUtils.waitForGlobalNotificationBar(win, PORTAL_NOTIFICATION_VALUE);
  }
  // Then we see if there's already a portal tab. If it's open, it'll be the second one.
  let tab = win.gBrowser.tabs[1];
  if (!tab || tab.linkedBrowser.currentURI.spec != CANONICAL_URL) {
    // The tab either hasn't been opened yet or it hasn't loaded the portal URL.
    // Waiting for a location change in the tabbrowser covers both cases.
    yield BrowserTestUtils.waitForLocationChange(win.gBrowser, CANONICAL_URL);
    // At this point the portal tab should be the second tab. If there is still
    // no second tab, something is wrong, and the selectedTab test below will fail.
    tab = win.gBrowser.tabs[1];
  }
  is(win.gBrowser.selectedTab, tab,
    "The captive portal tab should be open and selected in the new window.");
  testShowLoginPageButtonVisibility(notification, "hidden");
  return win;
}

function freePortal(aSuccess) {
  Services.obs.notifyObservers(null,
    "captive-portal-login-" + (aSuccess ? "success" : "abort"), null);
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

// Each of the test cases below is run twice: once for login-success and once
// for login-abort (aSuccess set to true and false respectively).
let testCasesForBothSuccessAndAbort = [
  /**
   * A portal is detected when there's no browser window,
   * then a browser window is opened, then the portal is freed.
   * The portal tab should be added and focused when the window is
   * opened, and closed automatically when the success event is fired.
   */
  function* test_detectedWithNoBrowserWindow_Open(aSuccess) {
    yield portalDetectedNoBrowserWindow();
    let win = yield openWindowAndWaitForPortalTabAndNotification();
    freePortal(aSuccess);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    yield BrowserTestUtils.closeWindow(win);
  },

  /**
   * A portal is detected when there's no browser window, and the
   * portal is freed before a browser window is opened. No portal
   * tab should be added when a browser window is opened.
   */
  function* test_detectedWithNoBrowserWindow_GoneBeforeOpen(aSuccess) {
    yield portalDetectedNoBrowserWindow();
    freePortal(aSuccess);
    let win = yield BrowserTestUtils.openNewBrowserWindow();
    // Wait for a while to make sure no tab is opened.
    yield new Promise(resolve => {
      setTimeout(resolve, 1000);
    });
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    yield BrowserTestUtils.closeWindow(win);
  },

  /**
   * A portal is detected when a browser window has focus. A portal tab should be
   * opened in the background in the focused browser window. If the portal is
   * freed when the tab isn't focused, the tab should be closed automatically.
   */
  function* test_detectedWithFocus(aSuccess) {
    let win = RecentWindow.getMostRecentBrowserWindow();
    let p = BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
    Services.obs.notifyObservers(null, "captive-portal-login", null);
    let tab = yield p;
    ensurePortalTab(win);
    ensurePortalNotification(win);
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    freePortal(aSuccess);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
  },

  /**
   * A portal is detected when a browser window has focus. A portal tab should be
   * opened in the background in the focused browser window. If the portal is
   * freed when the tab has focus, the tab should be closed automatically.
   */
  function* test_detectedWithFocus_selectedTab(aSuccess) {
    let win = RecentWindow.getMostRecentBrowserWindow();
    let p = BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
    Services.obs.notifyObservers(null, "captive-portal-login", null);
    let tab = yield p;
    ensurePortalTab(win);
    ensurePortalNotification(win);
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    win.gBrowser.selectedTab = tab;
    freePortal(aSuccess);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
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
    yield portalDetectedNoBrowserWindow();
    let win = yield openWindowAndWaitForPortalTabAndNotification();
    let browser = win.gBrowser.selectedTab.linkedBrowser;
    let loadPromise =
      BrowserTestUtils.browserLoaded(browser, false, CANONICAL_URL_REDIRECTED);
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    yield loadPromise;
    freePortal(true);
    ensurePortalTab(win);
    ensureNoPortalNotification(win);
    yield BrowserTestUtils.closeWindow(win);
  },

  /**
   * A portal is detected when a browser window has focus. A portal tab should be
   * opened in the background in the focused browser window. If the portal is
   * freed when the tab isn't focused, the tab should be closed automatically,
   * even if the portal has redirected to a URL other than CANONICAL_URL.
   */
  function* test_detectedWithFocus_redirectUnselectedTab() {
    let win = RecentWindow.getMostRecentBrowserWindow();
    let p = BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
    Services.obs.notifyObservers(null, "captive-portal-login", null);
    let tab = yield p;
    ensurePortalTab(win);
    ensurePortalNotification(win);
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    let browser = tab.linkedBrowser;
    let loadPromise =
      BrowserTestUtils.browserLoaded(browser, false, CANONICAL_URL_REDIRECTED);
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    yield loadPromise;
    freePortal(true);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
  },

  /**
   * A portal is detected when a browser window has focus. A portal tab should be
   * opened in the background in the focused browser window. If the portal is
   * freed when the tab has focus, and it has redirected to another page, the
   * tab should be kept open.
   */
  function* test_detectedWithFocus_redirectSelectedTab() {
    let win = RecentWindow.getMostRecentBrowserWindow();
    let p = BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
    Services.obs.notifyObservers(null, "captive-portal-login", null);
    let tab = yield p;
    ensurePortalNotification(win);
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    win.gBrowser.selectedTab = tab;
    let browser = tab.linkedBrowser;
    let loadPromise =
      BrowserTestUtils.browserLoaded(browser, false, CANONICAL_URL_REDIRECTED);
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    yield loadPromise;
    freePortal(true);
    ensurePortalTab(win);
    ensureNoPortalNotification(win);
    yield BrowserTestUtils.removeTab(tab);
  },

  /**
   * Test the various expected behaviors of the "Show Login Page" button
   * in the captive portal notification. The button should be visible for
   * all tabs except the captive portal tab, and when clicked, should
   * ensure a captive portal tab is open and select it.
   */
  function* test_showLoginPageButton() {
    let win = RecentWindow.getMostRecentBrowserWindow();
    let p = BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
    Services.obs.notifyObservers(null, "captive-portal-login", null);
    let tab = yield p;
    let notification = ensurePortalNotification(win);
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    testShowLoginPageButtonVisibility(notification, "visible");

    function testPortalTabSelectedAndButtonNotVisible() {
      is(win.gBrowser.selectedTab, tab, "The captive portal tab should be selected.");
      testShowLoginPageButtonVisibility(notification, "hidden");
    }

    // Select the captive portal tab. The button should hide.
    let otherTab = win.gBrowser.selectedTab;
    win.gBrowser.selectedTab = tab;
    testShowLoginPageButtonVisibility(notification, "hidden");

    // Select the other tab. The button should become visible.
    win.gBrowser.selectedTab = otherTab;
    testShowLoginPageButtonVisibility(notification, "visible");

    // Simulate clicking the button. The portal tab should be selected and
    // the button should hide.
    let button = notification.querySelector("button.notification-button");
    button.click();
    testPortalTabSelectedAndButtonNotVisible();

    // Close the tab. The button should become visible.
    yield BrowserTestUtils.removeTab(tab);
    ensureNoPortalTab(win);
    testShowLoginPageButtonVisibility(notification, "visible");

    function* clickButtonAndExpectNewPortalTab() {
      p = BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
      button.click();
      tab = yield p;
      is(win.gBrowser.selectedTab, tab, "The captive portal tab should be selected.");
    }

    // When the button is clicked, a new portal tab should be opened and
    // selected.
    yield clickButtonAndExpectNewPortalTab();

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
    yield clickButtonAndExpectNewPortalTab();

    yield BrowserTestUtils.removeTab(anotherTab);
    freePortal(true);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
  },
];

for (let testcase of testCasesForBothSuccessAndAbort) {
  add_task(testcase.bind(null, true));
  add_task(testcase.bind(null, false));
}

for (let testcase of singleRunTestCases) {
  add_task(testcase);
}
