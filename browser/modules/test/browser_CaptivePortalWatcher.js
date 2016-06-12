"use strict";

Components.utils.import("resource:///modules/RecentWindow.jsm");

const CANONICAL_CONTENT = "success";
const CANONICAL_URL = "data:text/plain;charset=utf-8," + CANONICAL_CONTENT;
const CANONICAL_URL_REDIRECTED = "data:text/plain;charset=utf-8,redirected";

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

function* openWindowAndWaitForPortalTab() {
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let tab = yield BrowserTestUtils.waitForNewTab(win.gBrowser, CANONICAL_URL);
  is(win.gBrowser.selectedTab, tab,
    "The captive portal tab should be open and selected in the new window.");
  return win;
}

function freePortal(aSuccess) {
  Services.obs.notifyObservers(null,
    "captive-portal-login-" + (aSuccess ? "success" : "abort"), null);
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
    let win = yield openWindowAndWaitForPortalTab();
    freePortal(aSuccess);
    is(win.gBrowser.tabs.length, 1,
      "The captive portal tab should have been closed.");
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
    is(win.gBrowser.tabs.length, 1,
      "No captive portal tab should have been opened.");
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
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    freePortal(aSuccess);
    is(win.gBrowser.tabs.length, 1,
      "The portal tab should have been closed.");
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
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    win.gBrowser.selectedTab = tab;
    freePortal(aSuccess);
    is(win.gBrowser.tabs.length, 1,
      "The portal tab should have been closed.");
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
    let win = yield openWindowAndWaitForPortalTab();
    let browser = win.gBrowser.selectedTab.linkedBrowser;
    let loadPromise =
      BrowserTestUtils.browserLoaded(browser, false, CANONICAL_URL_REDIRECTED);
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    yield loadPromise;
    freePortal(true);
    is(win.gBrowser.tabs.length, 2,
      "The captive portal tab should not have been closed.");
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
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    let browser = tab.linkedBrowser;
    let loadPromise =
      BrowserTestUtils.browserLoaded(browser, false, CANONICAL_URL_REDIRECTED);
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    yield loadPromise;
    freePortal(true);
    is(win.gBrowser.tabs.length, 1,
      "The portal tab should have been closed.");
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
    isnot(win.gBrowser.selectedTab, tab,
      "The captive portal tab should be open in the background in the current window.");
    win.gBrowser.selectedTab = tab;
    let browser = tab.linkedBrowser;
    let loadPromise =
      BrowserTestUtils.browserLoaded(browser, false, CANONICAL_URL_REDIRECTED);
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    yield loadPromise;
    freePortal(true);
    is(win.gBrowser.tabs.length, 2,
      "The portal tab should not have been closed.");
    yield BrowserTestUtils.removeTab(tab);
  },
];

for (let testcase of testCasesForBothSuccessAndAbort) {
  add_task(testcase.bind(null, true));
  add_task(testcase.bind(null, false));
}

for (let testcase of singleRunTestCases) {
  add_task(testcase);
}
