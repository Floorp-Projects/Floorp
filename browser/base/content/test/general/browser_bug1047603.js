/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const OPEN_LOCATION_PREF = "browser.link.open_newwindow";
const NON_REMOTE_PAGE = "about:crashes";

const SIMPLE_PAGE_HTML = `
<a href="about:home" target="_blank" id="testAnchor">Open a window</a>
`;

function frame_script() {
  addMessageListener("test:click", (message) => {
    let element = content.document.getElementById("testAnchor");
    element.click();
  });
  sendAsyncMessage("test:ready");
}

/**
 * Returns a Promise that resolves once the frame_script is loaded
 * in the browser, and has seen the DOMContentLoaded event.
 */
function waitForFrameScriptReady(mm) {
  return new Promise((resolve, reject) => {
    mm.addMessageListener("test:ready", function onTestReady() {
      mm.removeMessageListener("test:ready", onTestReady);
      resolve();
    });
  });
}

/**
 * Takes some browser in some window, and forces that browser
 * to become non-remote, and then navigates it to a page that
 * we're not supposed to be displaying remotely. Returns a
 * Promise that resolves when the browser is no longer remote.
 */
function prepareNonRemoteBrowser(aWindow, browser) {
  aWindow.gBrowser.updateBrowserRemoteness(browser, false);
  browser.loadURI(NON_REMOTE_PAGE);
  return new Promise((resolve, reject) => {
    waitForCondition(() => !browser.isRemoteBrowser, () => {
      resolve();
    }, "Waiting for browser to become non-remote");
  })
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(OPEN_LOCATION_PREF);
});

/**
 * Test that if we open a new tab from a link in a non-remote
 * browser in an e10s window, that the new tab's browser is also
 * not remote. Also tests with a private browsing window.
 */
add_task(function* test_new_tab() {
  let normalWindow = yield promiseOpenAndLoadWindow({
    remote: true
  }, true);
  let privateWindow = yield promiseOpenAndLoadWindow({
    remote: true,
    private: true,
  }, true);

  for (let testWindow of [normalWindow, privateWindow]) {
    let testBrowser = testWindow.gBrowser.selectedBrowser;
    yield prepareNonRemoteBrowser(testWindow, testBrowser);

    // Get our framescript ready
    let mm = testBrowser.messageManager;
    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);
    let readyPromise = waitForFrameScriptReady(mm);
    yield readyPromise;

    // Inject our test HTML into our non-remote tab.
    testBrowser.contentDocument.body.innerHTML = SIMPLE_PAGE_HTML;

    // Click on the link in the browser, and wait for the new tab.
    mm.sendAsyncMessage("test:click");
    let tabOpenEvent = yield waitForNewTab(testWindow.gBrowser);
    let newTab = tabOpenEvent.target;
    ok(!newTab.linkedBrowser.isRemoteBrowser,
       "The opened browser should not be remote.");

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
  let normalWindow = yield promiseOpenAndLoadWindow({
    remote: true
  }, true);
  let privateWindow = yield promiseOpenAndLoadWindow({
    remote: true,
    private: true,
  }, true);

  // Fiddle with the prefs so that we open target="_blank" links
  // in new windows instead of new tabs.
  Services.prefs.setIntPref(OPEN_LOCATION_PREF,
                            Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW);

  for (let testWindow of [normalWindow, privateWindow]) {
    let testBrowser = testWindow.gBrowser.selectedBrowser;
    yield prepareNonRemoteBrowser(testWindow, testBrowser);

    // Get our framescript ready
    let mm = testBrowser.messageManager;
    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);
    let readyPromise = waitForFrameScriptReady(mm);
    yield readyPromise;

    // Inject our test HTML into our non-remote window.
    testBrowser.contentDocument.body.innerHTML = SIMPLE_PAGE_HTML;

    // Click on the link in the browser, and wait for the new window.
    let windowOpenPromise = promiseTopicObserved("browser-delayed-startup-finished");
    mm.sendAsyncMessage("test:click");
    let [newWindow] = yield windowOpenPromise;
    ok(!newWindow.gMultiProcessBrowser,
       "The opened window should not be an e10s window.");
    newWindow.close();
  }

  normalWindow.close();
  privateWindow.close();

  Services.prefs.clearUserPref(OPEN_LOCATION_PREF);
});
