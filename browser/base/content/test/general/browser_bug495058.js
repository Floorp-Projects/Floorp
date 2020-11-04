/**
 * Tests that the right elements of a tab are focused when it is
 * torn out into its own window.
 */

const URIS = ["about:blank", "about:sessionrestore", "about:privatebrowsing"];

add_task(async function() {
  for (let uri of URIS) {
    let tab = BrowserTestUtils.addTab(gBrowser);
    await BrowserTestUtils.loadURI(tab.linkedBrowser, uri);

    let win = gBrowser.replaceTabWithWindow(tab);

    let contentPainted = Promise.resolve();
    // In the e10s case, we wait for the content to first paint before we focus
    // the URL in the new window, to optimize for content paint time.
    if (tab.linkedBrowser.isRemoteBrowser) {
      contentPainted = BrowserTestUtils.waitForContentEvent(
        tab.linkedBrowser,
        "MozAfterPaint"
      );
    }

    await TestUtils.topicObserved(
      "browser-delayed-startup-finished",
      subject => subject == win
    );
    await contentPainted;
    tab = win.gBrowser.selectedTab;

    Assert.equal(
      win.gBrowser.currentURI.spec,
      uri,
      uri + ": uri loaded in detached tab"
    );
    Assert.equal(
      win.document.activeElement,
      win.gBrowser.selectedBrowser,
      uri + ": browser is focused"
    );
    Assert.equal(win.gURLBar.value, "", uri + ": urlbar is empty");
    Assert.ok(win.gURLBar.placeholder, uri + ": placeholder text is present");

    await BrowserTestUtils.closeWindow(win);
  }
});
