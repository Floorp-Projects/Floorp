const TEST_PAGE = `data:text/html,<html><body><a href="about:blank" target="_blank">Test</a></body></html>`;
const { CHROME_ALL, CHROME_REMOTE_WINDOW, CHROME_FISSION_WINDOW } =
  Ci.nsIWebBrowserChrome;

/**
 * Tests that when we open new browser windows from content they
 * get the full browser chrome.
 */
add_task(async function () {
  // Make sure that the window.open call will open a new
  // window instead of a new tab.
  await new Promise(resolve => {
    SpecialPowers.pushPrefEnv(
      {
        set: [["browser.link.open_newwindow", 2]],
      },
      resolve
    );
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async function (browser) {
      let openedPromise = BrowserTestUtils.waitForNewWindow();
      BrowserTestUtils.synthesizeMouse("a", 0, 0, {}, browser);
      let win = await openedPromise;

      let chromeFlags = win.docShell.treeOwner
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIAppWindow).chromeFlags;

      let expected = CHROME_ALL;

      // In the multi-process tab case, the new window will have the
      // CHROME_REMOTE_WINDOW flag set.
      if (gMultiProcessBrowser) {
        expected |= CHROME_REMOTE_WINDOW;
      }

      // In the multi-process subframe case, the new window will have the
      // CHROME_FISSION_WINDOW flag set.
      if (gFissionBrowser) {
        expected |= CHROME_FISSION_WINDOW;
      }

      is(chromeFlags, expected, "Window should have opened with all chrome");

      await BrowserTestUtils.closeWindow(win);
    }
  );
});
