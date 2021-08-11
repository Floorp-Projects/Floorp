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
  async function test_detectedWithNoBrowserWindow_Redirect() {
    await portalDetected();
    let win = await focusWindowAndWaitForPortalUI();
    let browser = win.gBrowser.selectedTab.linkedBrowser;
    let loadPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      CANONICAL_URL_REDIRECTED
    );
    BrowserTestUtils.loadURI(browser, CANONICAL_URL_REDIRECTED);
    await loadPromise;
    await freePortal(true);
    ensurePortalTab(win);
    ensureNoPortalNotification(win);
    await closeWindowAndWaitForWindowActivate(win);
  },

  /**
   * Test the various expected behaviors of the "Show Login Page" button
   * in the captive portal notification. The button should be visible for
   * all tabs except the captive portal tab, and when clicked, should
   * ensure a captive portal tab is open and select it.
   */
  async function test_showLoginPageButton() {
    let win = await openWindowAndWaitForFocus();
    await portalDetected();
    let notification = ensurePortalNotification(win);
    testShowLoginPageButtonVisibility(notification, "visible");

    function testPortalTabSelectedAndButtonNotVisible() {
      is(
        win.gBrowser.selectedTab,
        tab,
        "The captive portal tab should be selected."
      );
      testShowLoginPageButtonVisibility(notification, "hidden");
    }

    let button = notification.buttonContainer.querySelector(
      "button.notification-button"
    );
    async function clickButtonAndExpectNewPortalTabAndNotification() {
      let newTabCreation = BrowserTestUtils.waitForNewTab(
        win.gBrowser,
        CANONICAL_URL
      );
      let buttonPressedEvent = TestUtils.topicObserved(
        "captive-portal-login-button-pressed"
      );

      button.click();

      const [tab] = await Promise.all([newTabCreation, buttonPressedEvent]);
      is(
        win.gBrowser.selectedTab,
        tab,
        "The captive portal tab should be selected."
      );
      return tab;
    }

    // Simulate clicking the button. The portal tab should be opened and
    // selected and the button should hide.
    let tab = await clickButtonAndExpectNewPortalTabAndNotification();
    testPortalTabSelectedAndButtonNotVisible();

    // Close the tab. The button should become visible.
    BrowserTestUtils.removeTab(tab);
    ensureNoPortalTab(win);
    testShowLoginPageButtonVisibility(notification, "visible");

    // When the button is clicked, a new portal tab should be opened and
    // selected.
    tab = await clickButtonAndExpectNewPortalTabAndNotification();

    // Open another arbitrary tab. The button should become visible. When it's clicked,
    // the portal tab should be selected.
    let anotherTab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser);
    testShowLoginPageButtonVisibility(notification, "visible");
    button.click();
    is(
      win.gBrowser.selectedTab,
      tab,
      "The captive portal tab should be selected."
    );

    // Close the portal tab and select the arbitrary tab. The button should become
    // visible and when it's clicked, a new portal tab should be opened.
    BrowserTestUtils.removeTab(tab);
    win.gBrowser.selectedTab = anotherTab;
    testShowLoginPageButtonVisibility(notification, "visible");
    tab = await clickButtonAndExpectNewPortalTabAndNotification();

    BrowserTestUtils.removeTab(anotherTab);
    await freePortal(true);
    ensureNoPortalTab(win);
    ensureNoPortalNotification(win);
    await closeWindowAndWaitForWindowActivate(win);
  },
  async function testLoginSuccessAfterButtonPress() {
    // test that button not clicked generates no notification
    let loginSuccessAfterButtonPress = TestUtils.topicObserved(
      "captive-portal-login-success-after-button-pressed"
    );
    window.CaptivePortalWatcher.observe(null, "captive-portal-login-success");

    let exception = undefined;
    try {
      await waitForPromiseWithTimeout(loginSuccessAfterButtonPress);
    } catch (ex) {
      exception = ex;
    }
    isnot(
      exception,
      undefined,
      "captive-portal-login-success-after-button-pressed should not have" +
        " been sent, because button was not pressed"
    );

    // test that button clicked does generate a notification
    loginSuccessAfterButtonPress = TestUtils.topicObserved(
      "captive-portal-login-success-after-button-pressed"
    );

    window.CaptivePortalWatcher.observe(
      null,
      "captive-portal-login-button-pressed"
    );
    window.CaptivePortalWatcher.observe(null, "captive-portal-login-success");

    await loginSuccessAfterButtonPress;

    // test that subsequent captive-portal-login-success without a click
    // does not send a notification

    // Normally, we would stub Cu.now() and make it move the clock forward.
    // Unfortunately, since it's in C++, we can't do that here, so instead
    // we move the time stamp into the past so that things will have expired.
    window.CaptivePortalWatcher._loginButtonPressedTimeStamp -=
      window.CaptivePortalWatcher._LOGIN_BUTTON_PRESSED_TIMEOUT;

    loginSuccessAfterButtonPress = TestUtils.topicObserved(
      "captive-portal-login-success-after-button-pressed"
    );

    window.CaptivePortalWatcher.observe(null, "captive-portal-login-success");
    exception = undefined;
    try {
      await waitForPromiseWithTimeout(loginSuccessAfterButtonPress);
    } catch (ex) {
      exception = ex;
    }

    isnot(
      exception,
      undefined,
      "captive-portal-login-success-after-button-pressed should not have" +
        " been sent, button was not pressed again"
    );
  },
];

for (let testcase of testcases) {
  add_task(testcase);
}
