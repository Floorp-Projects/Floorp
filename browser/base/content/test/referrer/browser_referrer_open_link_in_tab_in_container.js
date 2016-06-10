// Tests referrer on context menu navigation - open link in new tab.
// Selects "open link in new tab" from the context menu.

// This test starts from a container tab. We don't want to propagate the
// referrer.
function getReferrerTest(aTestNumber) {
  let test = _referrerTests[aTestNumber];
  if (test) {
    // We want all the referrer tests to fail!
    test.result = "";
  }

  return test;
}

function startNewTabTestCase(aTestNumber) {
  info("browser_referrer_open_link_in_tab: " +
       getReferrerTestDescription(aTestNumber));
  contextMenuOpened(gTestWindow, "testlink").then(function(aContextMenu) {
    someTabLoaded(gTestWindow).then(function(aNewTab) {
      gTestWindow.gBrowser.selectedTab = aNewTab;
      checkReferrerAndStartNextTest(aTestNumber, null, aNewTab,
                                    startNewTabTestCase,
                                    { userContextId: 4 });
    });

    doContextMenuCommand(gTestWindow, aContextMenu, "context-openlinkintab");
  });
}

function test() {
  requestLongerTimeout(10);  // slowwww shutdown on e10s
  startReferrerTest(startNewTabTestCase, { userContextId: 4 });
}
