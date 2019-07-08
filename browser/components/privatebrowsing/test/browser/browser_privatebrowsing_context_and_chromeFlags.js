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
  let winDocShell = win.docShell;
  let chromeFlags = winDocShell.treeOwner
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIXULWindow).chromeFlags;

  if (!win.gBrowser.selectedBrowser.hasContentOpener) {
    Assert.ok(
      chromeFlags & Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
      "Should have the private window chrome flag"
    );
  }

  let loadContext = winDocShell.QueryInterface(Ci.nsILoadContext);
  Assert.ok(
    loadContext.usePrivateBrowsing,
    "The parent window should be using private browsing"
  );

  return ContentTask.spawn(
    win.gBrowser.selectedBrowser,
    null,
    async function() {
      let contentLoadContext = docShell.QueryInterface(Ci.nsILoadContext);
      Assert.ok(
        contentLoadContext.usePrivateBrowsing,
        "Content docShell should be using private browsing"
      );
    }
  );
}

/**
 * Tests that chromeFlags bits and the nsILoadContext.usePrivateBrowsing
 * attribute are properly set when opening a new private browsing
 * window.
 */
add_task(async function test_context_and_chromeFlags() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await assertWindowIsPrivate(win);

  let browser = win.gBrowser.selectedBrowser;

  let newWinPromise = BrowserTestUtils.waitForNewWindow();
  await ContentTask.spawn(browser, null, async function() {
    content.open("http://example.com", "_blank", "width=100,height=100");
  });

  let win2 = await newWinPromise;
  await assertWindowIsPrivate(win2);

  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win);
});
