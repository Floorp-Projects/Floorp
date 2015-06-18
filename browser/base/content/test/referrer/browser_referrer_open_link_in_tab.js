// Tests referrer on context menu navigation - open link in new tab.
// Selects "open link in new tab" from the context menu.

function startNewTabTestCase(aTestNumber) {
  info("browser_referrer_open_link_in_tab: " +
       getReferrerTestDescription(aTestNumber));
  contextMenuOpened(gTestWindow, "testlink").then(function(aContextMenu) {
    someTabLoaded(gTestWindow).then(function(aNewTab) {
      gTestWindow.gBrowser.selectedTab = aNewTab;
      checkReferrerAndStartNextTest(aTestNumber, null, aNewTab,
                                    startNewTabTestCase);
    });

    doContextMenuCommand(gTestWindow, aContextMenu, "context-openlinkintab");
  });
}

function test() {
  requestLongerTimeout(10);  // slowwww shutdown on e10s
  startReferrerTest(startNewTabTestCase);
}
