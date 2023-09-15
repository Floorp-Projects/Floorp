/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const OPEN_LOCATION_PREF = "browser.link.open_newwindow";
const NON_REMOTE_PAGE = "about:welcomeback";

requestLongerTimeout(2);

function insertAndClickAnchor(browser) {
  return SpecialPowers.spawn(browser, [], () => {
    content.document.body.innerHTML = `
      <a href="http://example.com/" target="_blank" rel="opener" id="testAnchor">Open a window</a>
    `;

    let element = content.document.getElementById("testAnchor");
    element.click();
  });
}

/**
 * Takes some browser in some window, and forces that browser
 * to become non-remote, and then navigates it to a page that
 * we're not supposed to be displaying remotely. Returns a
 * Promise that resolves when the browser is no longer remote.
 */
function prepareNonRemoteBrowser(aWindow, browser) {
  BrowserTestUtils.startLoadingURIString(browser, NON_REMOTE_PAGE);
  return BrowserTestUtils.browserLoaded(browser);
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(OPEN_LOCATION_PREF);
});

/**
 * Test that if we open a new tab from a link in a non-remote
 * browser in an e10s window, that the new tab will load properly.
 */
add_task(async function test_new_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.skip_html_fragment_assertion", true]],
  });

  let normalWindow = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
  });
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    remote: true,
    private: true,
  });

  for (let testWindow of [normalWindow, privateWindow]) {
    await promiseWaitForFocus(testWindow);
    let testBrowser = testWindow.gBrowser.selectedBrowser;
    info("Preparing non-remote browser");
    await prepareNonRemoteBrowser(testWindow, testBrowser);
    info("Non-remote browser prepared");

    let tabOpenEventPromise = waitForNewTabEvent(testWindow.gBrowser);
    await insertAndClickAnchor(testBrowser);

    let newTab = (await tabOpenEventPromise).target;
    await promiseTabLoadEvent(newTab);

    // insertAndClickAnchor causes an open to a web page which
    // means that the tab should eventually become remote.
    ok(
      newTab.linkedBrowser.isRemoteBrowser,
      "The opened browser never became remote."
    );

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
add_task(async function test_new_window() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.skip_html_fragment_assertion", true]],
  });

  let normalWindow = await BrowserTestUtils.openNewBrowserWindow(
    {
      remote: true,
    },
    true
  );
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow(
    {
      remote: true,
      private: true,
    },
    true
  );

  // Fiddle with the prefs so that we open target="_blank" links
  // in new windows instead of new tabs.
  Services.prefs.setIntPref(
    OPEN_LOCATION_PREF,
    Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW
  );

  for (let testWindow of [normalWindow, privateWindow]) {
    await promiseWaitForFocus(testWindow);
    let testBrowser = testWindow.gBrowser.selectedBrowser;
    await prepareNonRemoteBrowser(testWindow, testBrowser);

    await insertAndClickAnchor(testBrowser);

    // Click on the link in the browser, and wait for the new window.
    let [newWindow] = await TestUtils.topicObserved(
      "browser-delayed-startup-finished"
    );

    is(
      PrivateBrowsingUtils.isWindowPrivate(testWindow),
      PrivateBrowsingUtils.isWindowPrivate(newWindow),
      "Private browsing state of new window does not match the original!"
    );

    let newTab = newWindow.gBrowser.selectedTab;

    await promiseTabLoadEvent(newTab);

    // insertAndClickAnchor causes an open to a web page which
    // means that the tab should eventually become remote.
    ok(
      newTab.linkedBrowser.isRemoteBrowser,
      "The opened browser never became remote."
    );
    newWindow.close();
  }

  normalWindow.close();
  privateWindow.close();
});
