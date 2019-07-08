// Tests referrer on context menu navigation - open link in new private window.
// Selects "open link in new private window" from the context menu.

// The test runs from a regular browsing window.
// Output: we have no referrer.

getReferrerTest = getRemovedReferrerTest;

function startNewPrivateWindowTestCase(aTestNumber) {
  info(
    "browser_referrer_open_link_in_private: " +
      getReferrerTestDescription(aTestNumber)
  );
  contextMenuOpened(gTestWindow, "testlink").then(function(aContextMenu) {
    newWindowOpened().then(function(aNewWindow) {
      BrowserTestUtils.firstBrowserLoaded(aNewWindow, false).then(function() {
        checkReferrerAndStartNextTest(
          aTestNumber,
          aNewWindow,
          null,
          startNewPrivateWindowTestCase
        );
      });
    });

    doContextMenuCommand(gTestWindow, aContextMenu, "context-openlinkprivate");
  });
}

function test() {
  requestLongerTimeout(10); // slowwww shutdown on e10s
  startReferrerTest(startNewPrivateWindowTestCase);
}
