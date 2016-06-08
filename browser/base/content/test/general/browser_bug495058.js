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

    yield Promise.all([delayedStartup, contentPainted]);

    Assert.equal(win.gBrowser.currentURI.spec, uri, uri + ": uri loaded in detached tab");
    Assert.equal(win.document.activeElement, win.gBrowser.selectedBrowser, uri + ": browser is focused");
    Assert.equal(win.gURLBar.value, "", uri + ": urlbar is empty");
    Assert.ok(win.gURLBar.placeholder, uri + ": placeholder text is present");

    yield BrowserTestUtils.closeWindow(win);
  }
});
