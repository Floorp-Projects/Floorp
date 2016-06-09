// Tests referrer on context menu navigation - open link in new container tab.
// Selects "open link in new container tab" from the context menu.

// The test runs from a container ID 1.
// Output: we have the correct referrer policy applied.

function startNewTabTestCase(aTestNumber) {
  info("browser_referrer_open_link_in_container_tab: " +
       getReferrerTestDescription(aTestNumber));
  contextMenuOpened(gTestWindow, "testlink").then(function(aContextMenu) {
    someTabLoaded(gTestWindow).then(function(aNewTab) {
      gTestWindow.gBrowser.selectedTab = aNewTab;

      checkReferrerAndStartNextTest(aTestNumber, null, aNewTab,
                                    startNewTabTestCase, { userContextId: 1 });
    });

    doContextMenuCommand(gTestWindow, aContextMenu, "context-openlinkincontainertab");
  });
}

function test() {
  waitForExplicitFinish();

  SpecialPowers.pushPrefEnv(
    {set: [["privacy.userContext.enabled", true]]},
    function() {
      requestLongerTimeout(10);  // slowwww shutdown on e10s
      startReferrerTest(startNewTabTestCase, { userContextId: 1 });
    });
}
