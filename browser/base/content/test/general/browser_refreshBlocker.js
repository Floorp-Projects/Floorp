"use strict";

const META_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org/browser/browser/base/content/test/general/refresh_meta.sjs";
const HEADER_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org/browser/browser/base/content/test/general/refresh_header.sjs";
const TARGET_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
const PREF = "accessibility.blockautorefresh";

/**
 * Goes into the content, and simulates a meta-refresh header at a very
 * low level, and checks to see if it was blocked. This will always cancel
 * the refresh, regardless of whether or not the refresh was blocked.
 *
 * @param browser (<xul:browser>)
 *        The browser to test for refreshing.
 * @param expectRefresh (bool)
 *        Whether or not we expect the refresh attempt to succeed.
 * @returns Promise
 */
async function attemptFakeRefresh(browser, expectRefresh) {
  await SpecialPowers.spawn(
    browser,
    [expectRefresh],
    async function (contentExpectRefresh) {
      let URI = docShell.QueryInterface(Ci.nsIWebNavigation).currentURI;
      let refresher = docShell.QueryInterface(Ci.nsIRefreshURI);
      refresher.refreshURI(URI, null, 0);

      Assert.equal(
        refresher.refreshPending,
        contentExpectRefresh,
        "Got the right refreshPending state"
      );

      if (refresher.refreshPending) {
        // Cancel the pending refresh
        refresher.cancelRefreshURITimers();
      }

      // The RefreshBlocker will wait until onLocationChange has
      // been fired before it will show any notifications (see bug
      // 1246291), so we cause this to occur manually here.
      content.location = URI.spec + "#foo";
    }
  );
}

/**
 * Tests that we can enable the blocking pref and block a refresh
 * from occurring while showing a notification bar. Also tests that
 * when we disable the pref, that refreshes can go through again.
 */
add_task(async function test_can_enable_and_block() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TARGET_PAGE,
    },
    async function (browser) {
      // By default, we should be able to reload the page.
      await attemptFakeRefresh(browser, true);

      await pushPrefs(["accessibility.blockautorefresh", true]);

      let notificationPromise = BrowserTestUtils.waitForNotificationBar(
        gBrowser,
        browser,
        "refresh-blocked"
      );

      await attemptFakeRefresh(browser, false);

      await notificationPromise;

      await pushPrefs(["accessibility.blockautorefresh", false]);

      // Page reloads should go through again.
      await attemptFakeRefresh(browser, true);
    }
  );
});

/**
 * Attempts a "real" refresh by opening a tab, and then sending it to
 * an SJS page that will attempt to cause a refresh. This will also pass
 * a delay amount to the SJS page. The refresh should be blocked, and
 * the notification should be shown. Once shown, the "Allow" button will
 * be clicked, and the refresh will go through. Finally, the helper will
 * close the tab and resolve the Promise.
 *
 * @param refreshPage (string)
 *        The SJS page to use. Use META_PAGE for the <meta> tag refresh
 *        case. Use HEADER_PAGE for the HTTP header case.
 * @param delay (int)
 *        The amount, in ms, for the page to wait before attempting the
 *        refresh.
 *
 * @returns Promise
 */
async function testRealRefresh(refreshPage, delay) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async function (browser) {
      await pushPrefs(["accessibility.blockautorefresh", true]);

      BrowserTestUtils.startLoadingURIString(
        browser,
        refreshPage + "?p=" + TARGET_PAGE + "&d=" + delay
      );
      await BrowserTestUtils.browserLoaded(browser);

      // Once browserLoaded resolves, all nsIWebProgressListener callbacks
      // should have fired, so the notification should be visible.
      let notificationBox = gBrowser.getNotificationBox(browser);
      let notification = notificationBox.currentNotification;

      ok(notification, "Notification should be visible");
      is(
        notification.getAttribute("value"),
        "refresh-blocked",
        "Should be showing the right notification"
      );

      // Then click the button to allow the refresh.
      let buttons = notification.buttonContainer.querySelectorAll(
        ".notification-button"
      );
      is(buttons.length, 1, "Should have one button.");

      // Prepare a Promise that should resolve when the refresh goes through
      let refreshPromise = BrowserTestUtils.browserLoaded(browser);
      buttons[0].click();

      await refreshPromise;
    }
  );
}

/**
 * Tests the meta-tag case for both short and longer delay times.
 */
add_task(async function test_can_allow_refresh() {
  await testRealRefresh(META_PAGE, 0);
  await testRealRefresh(META_PAGE, 100);
  await testRealRefresh(META_PAGE, 500);
});

/**
 * Tests that when a HTTP header case for both short and longer
 * delay times.
 */
add_task(async function test_can_block_refresh_from_header() {
  await testRealRefresh(HEADER_PAGE, 0);
  await testRealRefresh(HEADER_PAGE, 100);
  await testRealRefresh(HEADER_PAGE, 500);
});

/**
 * Tests that we can update a notification when multiple reload/redirect
 * attempts happen.
 */
add_task(async function test_can_update_notification() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async function (browser) {
      await pushPrefs(["accessibility.blockautorefresh", true]);

      // First, attempt a redirect
      BrowserTestUtils.startLoadingURIString(
        browser,
        META_PAGE + "?d=0&p=" + TARGET_PAGE
      );
      await BrowserTestUtils.browserLoaded(browser);

      // Once browserLoaded resolves, all nsIWebProgressListener callbacks
      // should have fired, so the notification should be visible.
      let notificationBox = gBrowser.getNotificationBox(browser);
      let notification = notificationBox.currentNotification;

      let message = notification.messageText.querySelector("span");
      is(
        message.dataset.l10nId,
        "refresh-blocked-redirect-label",
        "Should be showing the redirect message"
      );

      // Next, attempt a refresh
      await attemptFakeRefresh(browser, false);

      message = notification.messageText.querySelector("span");
      is(
        message.dataset.l10nId,
        "refresh-blocked-refresh-label",
        "Should be showing the refresh message"
      );
    }
  );
});
