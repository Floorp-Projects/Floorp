/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const OPEN_LOCATION_PREF = "browser.link.open_newwindow";
const NON_REMOTE_PAGE = "about:welcomeback";

Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

requestLongerTimeout(2);

function frame_script() {
  content.document.body.innerHTML = `
    <a href="about:home" target="_blank" id="testAnchor">Open a window</a>
  `;

  let element = content.document.getElementById("testAnchor");
  element.click();
}

/**
 * Takes some browser in some window, and forces that browser
 * to become non-remote, and then navigates it to a page that
 * we're not supposed to be displaying remotely. Returns a
 * Promise that resolves when the browser is no longer remote.
 */
function prepareNonRemoteBrowser(aWindow, browser) {
  browser.loadURI(NON_REMOTE_PAGE);
  return BrowserTestUtils.browserLoaded(browser);
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(OPEN_LOCATION_PREF);
});

/**
 * Test that if we open a new tab from a link in a non-remote
 * browser in an e10s window, that the new tab will load properly.
 */
add_task(function* test_new_tab() {
  let normalWindow = yield BrowserTestUtils.openNewBrowserWindow({
    remote: true,
  });
  let privateWindow = yield BrowserTestUtils.openNewBrowserWindow({
    remote: true,
    private: true,
  });

  for (let testWindow of [normalWindow, privateWindow]) {
    yield promiseWaitForFocus(testWindow);
    let testBrowser = testWindow.gBrowser.selectedBrowser;
    info("Preparing non-remote browser");
    yield prepareNonRemoteBrowser(testWindow, testBrowser);
    info("Non-remote browser prepared - sending frame script");

    // Get our framescript ready
    let mm = testBrowser.messageManager;
    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);

    let tabOpenEvent = yield waitForNewTabEvent(testWindow.gBrowser);
    let newTab = tabOpenEvent.target;

    yield promiseTabLoadEvent(newTab);

    // Our framescript opens to about:home which means that the
    // tab should eventually become remote.
    ok(newTab.linkedBrowser.isRemoteBrowser,
       "The opened browser never became remote.");

    testWindow.gBrowser.removeTab(newTab);
  }

  normalWindow.close();
  privateWindow.close();
});

/**
 * Test that if we open a new window from a link in a non-remote
 * browser in an e10s window, that the new window is not an e10s
 * window. Also tests with a private browsing window.
 */
add_task(function* test_new_window() {
  let normalWindow = yield BrowserTestUtils.openNewBrowserWindow({
    remote: true
  }, true);
  let privateWindow = yield BrowserTestUtils.openNewBrowserWindow({
    remote: true,
    private: true,
  }, true);

  // Fiddle with the prefs so that we open target="_blank" links
  // in new windows instead of new tabs.
  Services.prefs.setIntPref(OPEN_LOCATION_PREF,
                            Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW);

  for (let testWindow of [normalWindow, privateWindow]) {
    yield promiseWaitForFocus(testWindow);
    let testBrowser = testWindow.gBrowser.selectedBrowser;
    yield prepareNonRemoteBrowser(testWindow, testBrowser);

    // Get our framescript ready
    let mm = testBrowser.messageManager;
    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);

    // Click on the link in the browser, and wait for the new window.
    let {subject: newWindow} =
      yield promiseTopicObserved("browser-delayed-startup-finished");

    is(PrivateBrowsingUtils.isWindowPrivate(testWindow),
       PrivateBrowsingUtils.isWindowPrivate(newWindow),
       "Private browsing state of new window does not match the original!");

    let newTab = newWindow.gBrowser.selectedTab;

    yield promiseTabLoadEvent(newTab);

    // Our framescript opens to about:home which means that the
    // tab should eventually become remote.
    ok(newTab.linkedBrowser.isRemoteBrowser,
       "The opened browser never became remote.");
    newWindow.close();
  }

  normalWindow.close();
  privateWindow.close();
});
