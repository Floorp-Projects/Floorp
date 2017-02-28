"use strict";

add_task(setupPrefsAndRecentWindowBehavior);

let testcases = [
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

for (let testcase of testcases) {
  add_task(testcase);
}
