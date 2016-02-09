"use strict";

const PAGE = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";
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
function* attemptRefresh(browser, expectRefresh) {
  yield ContentTask.spawn(browser, expectRefresh, function*(expectRefresh) {
    let URI = docShell.QueryInterface(Ci.nsIWebNavigation).currentURI;
    let refresher = docShell.QueryInterface(Ci.nsIRefreshURI);
    refresher.refreshURI(URI, 0, false, true);

    is(refresher.refreshPending, expectRefresh,
       "Got the right refreshPending state");

    if (refresher.refreshPending) {
      // Cancel the pending refresh
      refresher.cancelRefreshURITimers();
    }
  });
}

/**
 * Tests that we can enable the blocking pref and block a refresh
 * from occurring while showing a notification bar. Also tests that
 * when we disable the pref, that refreshes can go through again.
 */
add_task(function* test_can_enable_and_block() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, function*(browser) {
    // By default, we should be able to reload the page.
    yield attemptRefresh(browser, true);

    yield pushPrefs(["accessibility.blockautorefresh", true]);

    let notificationPromise =
      BrowserTestUtils.waitForNotificationBar(gBrowser, browser,
                                              "refresh-blocked");

    yield attemptRefresh(browser, false);

    yield notificationPromise;

    yield pushPrefs(["accessibility.blockautorefresh", false]);

    // Page reloads should go through again.
    yield attemptRefresh(browser, true);
  });
});

/**
 * Tests that when a refresh is blocked that we show a notification
 * bar, and that when we click on the "Allow" button, the refresh
 * can then go through.
 */
add_task(function* test_can_allow_refresh() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, function*(browser) {
    yield pushPrefs(["accessibility.blockautorefresh", true]);

    let notificationPromise =
      BrowserTestUtils.waitForNotificationBar(gBrowser, browser,
                                              "refresh-blocked");

    yield attemptRefresh(browser, false);
    let notification = yield notificationPromise;

    // Then click the button to submit the crash report.
    let buttons = notification.querySelectorAll(".notification-button");
    is(buttons.length, 1, "Should have one button.");

    // Prepare a Promise that should resolve when the refresh goes through
    let refreshPromise = BrowserTestUtils.browserLoaded(browser);
    buttons[0].click();

    yield refreshPromise;
  });
});
