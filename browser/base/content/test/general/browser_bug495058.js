/**
 * Tests that the right elements of a tab are focused when it is
 * torn out into its own window.
 */

const URIS = [
  "about:blank",
  "about:sessionrestore",
  "about:privatebrowsing",
];

add_task(function*() {
  for (let uri of URIS) {
    let tab = gBrowser.addTab();
    yield BrowserTestUtils.loadURI(tab.linkedBrowser, uri);

    let win = gBrowser.replaceTabWithWindow(tab);
    yield BrowserTestUtils.waitForEvent(win, "load");

    tab = win.gBrowser.selectedTab;

    // By default, we'll wait for MozAfterPaint to come up through the
    // browser element. We'll handle the e10s case in the next block.
    let contentPainted = BrowserTestUtils.waitForEvent(tab.linkedBrowser,
                                                       "MozAfterPaint");

    let delayedStartup =
      TestUtils.topicObserved("browser-delayed-startup-finished",
                              subject => subject == win);

    if (gMultiProcessBrowser &&
        E10SUtils.canLoadURIInProcess(uri, Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT)) {
      // Until bug 1261842 is fixed, the initial browser is going to be
      // non-remote. If we're transitioning to a URL that can be loaded
      // remotely, we'll need to wait until that remoteness switch is done
      // before we send any framescripts down to the browser.
      yield BrowserTestUtils.waitForEvent(tab, "TabRemotenessChange");
      contentPainted = BrowserTestUtils.contentPainted(tab.linkedBrowser);
    }

    yield Promise.all([delayedStartup, contentPainted]);

    Assert.equal(win.gBrowser.currentURI.spec, uri, uri + ": uri loaded in detached tab");
    Assert.equal(win.document.activeElement, win.gBrowser.selectedBrowser, uri + ": browser is focused");
    Assert.equal(win.gURLBar.value, "", uri + ": urlbar is empty");
    Assert.ok(win.gURLBar.placeholder, uri + ": placeholder text is present");

    yield BrowserTestUtils.closeWindow(win);
  }
});
