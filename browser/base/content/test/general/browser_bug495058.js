/**
 * Tests that the right elements of a tab are focused when it is
 * torn out into its own window.
 */

const URIS = [
  "about:blank",
  "about:sessionrestore",
  "about:privatebrowsing",
];

add_task(async function() {
  for (let uri of URIS) {
    let tab = BrowserTestUtils.addTab(gBrowser);
    await BrowserTestUtils.loadURI(tab.linkedBrowser, uri);

    let win = gBrowser.replaceTabWithWindow(tab);
    await TestUtils.topicObserved("browser-delayed-startup-finished",
                                  subject => subject == win);
    tab = win.gBrowser.selectedTab;

    // BrowserTestUtils doesn't get the add-on shims, which means that
    // MozAfterPaint won't get shimmed over if we add an event handler
    // for it in the parent.
    if (tab.linkedBrowser.isRemoteBrowser) {
      await BrowserTestUtils.waitForContentEvent(tab.linkedBrowser, "MozAfterPaint");
    } else {
      await BrowserTestUtils.waitForEvent(tab.linkedBrowser, "MozAfterPaint");
    }

    Assert.equal(win.gBrowser.currentURI.spec, uri, uri + ": uri loaded in detached tab");
    Assert.equal(win.document.activeElement, win.gBrowser.selectedBrowser, uri + ": browser is focused");
    Assert.equal(win.gURLBar.value, "", uri + ": urlbar is empty");
    Assert.ok(win.gURLBar.placeholder, uri + ": placeholder text is present");

    await BrowserTestUtils.closeWindow(win);
  }
});
