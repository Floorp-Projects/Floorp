/**
 * Tests that the right elements of a tab are focused when it is
 * torn out into its own window.
 */

const URIS = [
  "about:blank",
  "about:home",
  "about:sessionrestore",
  "about:privatebrowsing",
];

add_task(async function () {
  for (let uri of URIS) {
    let tab = BrowserTestUtils.addTab(gBrowser);
    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, uri);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    let isRemote = tab.linkedBrowser.isRemoteBrowser;

    let win = gBrowser.replaceTabWithWindow(tab);

    await TestUtils.topicObserved(
      "browser-delayed-startup-finished",
      subject => subject == win
    );
    // In the e10s case, we wait for the content to first paint before we focus
    // the URL in the new window, to optimize for content paint time.
    if (isRemote) {
      await win.gBrowserInit.firstContentWindowPaintPromise;
    }

    tab = win.gBrowser.selectedTab;

    Assert.equal(
      win.gBrowser.currentURI.spec,
      uri,
      uri + ": uri loaded in detached tab"
    );

    const expectedActiveElement = tab.isEmpty
      ? win.gURLBar.inputField
      : win.gBrowser.selectedBrowser;
    Assert.equal(
      win.document.activeElement,
      expectedActiveElement,
      `${uri}: the active element is expected: ${win.document.activeElement?.nodeName}`
    );
    Assert.equal(win.gURLBar.value, "", uri + ": urlbar is empty");
    Assert.ok(win.gURLBar.placeholder, uri + ": placeholder text is present");

    await BrowserTestUtils.closeWindow(win);
  }
});
