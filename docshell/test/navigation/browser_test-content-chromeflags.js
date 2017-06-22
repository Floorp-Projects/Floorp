const TEST_PAGE = `data:text/html,<html><body><a href="about:blank" target="_blank">Test</a></body></html>`;
const CHROME_ALL = Ci.nsIWebBrowserChrome.CHROME_ALL;
const CHROME_REMOTE_WINDOW = Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW;

/**
 * Tests that when we open new browser windows from content they
 * get the full browser chrome.
 */
add_task(async function() {
  // Make sure that the window.open call will open a new
  // window instead of a new tab.
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv({
      "set": [
        ["browser.link.open_newwindow", 2],
      ]
    }, resolve);
  });

  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_PAGE
  }, async function(browser) {
    let openedPromise = BrowserTestUtils.waitForNewWindow();
    BrowserTestUtils.synthesizeMouse("a", 0, 0, {}, browser);
    let win = await openedPromise;

    let chromeFlags = win.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShellTreeItem)
                         .treeOwner
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIXULWindow)
                         .chromeFlags;

    // In the multi-process case, the new window will have the
    // CHROME_REMOTE_WINDOW flag set.
    const EXPECTED = gMultiProcessBrowser ? CHROME_ALL | CHROME_REMOTE_WINDOW
                                          : CHROME_ALL;

    is(chromeFlags, EXPECTED, "Window should have opened with all chrome");

    BrowserTestUtils.closeWindow(win);
  });
});
