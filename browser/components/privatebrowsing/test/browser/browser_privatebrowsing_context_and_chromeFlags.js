"use strict";

/**
 * Given some window in the parent process, ensure that
 * the nsIXULWindow has the CHROME_PRIVATE_WINDOW chromeFlag,
 * and that the usePrivateBrowsing property is set to true on
 * both the window's nsILoadContext, as well as on the initial
 * browser's content docShell nsILoadContext.
 *
 * @param win (nsIDOMWindow)
 *        An nsIDOMWindow in the parent process.
 * @return Promise
 */
function assertWindowIsPrivate(win) {
  let winDocShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);
  let chromeFlags = winDocShell.QueryInterface(Ci.nsIDocShellTreeItem)
                               .treeOwner
                               .QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIXULWindow)
                               .chromeFlags;

  if (!win.gBrowser.selectedBrowser.hasContentOpener) {
    Assert.ok(chromeFlags & Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
              "Should have the private window chrome flag");
  }

  let loadContext = winDocShell.QueryInterface(Ci.nsILoadContext);
  Assert.ok(loadContext.usePrivateBrowsing,
            "The parent window should be using private browsing");

  return ContentTask.spawn(win.gBrowser.selectedBrowser, null, function*() {
    let loadContext = docShell.QueryInterface(Ci.nsILoadContext);
    Assert.ok(loadContext.usePrivateBrowsing,
              "Content docShell should be using private browsing");
  });
}

/**
 * Tests that chromeFlags bits and the nsILoadContext.usePrivateBrowsing
 * attribute are properly set when opening a new private browsing
 * window.
 */
add_task(function* test_context_and_chromeFlags() {
  let win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  yield assertWindowIsPrivate(win);

  let browser = win.gBrowser.selectedBrowser;

  let newWinPromise = BrowserTestUtils.waitForNewWindow();
  yield ContentTask.spawn(browser, null, function*() {
    content.open("http://example.com", "_blank", "width=100,height=100");
  });

  let win2 = yield newWinPromise;
  yield assertWindowIsPrivate(win2);

  yield BrowserTestUtils.closeWindow(win2);
  yield BrowserTestUtils.closeWindow(win);
});
