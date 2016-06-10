// Tests referrer on context menu navigation - open link in new window.
// Selects "open link in new window" from the context menu.

// This test runs from a container tab. The new tab/window will be loaded in
// the same container.

function startNewWindowTestCase(aTestNumber) {
  info("browser_referrer_open_link_in_window: " +
       getReferrerTestDescription(aTestNumber));
  contextMenuOpened(gTestWindow, "testlink").then(function(aContextMenu) {
    newWindowOpened().then(function(aNewWindow) {
      someTabLoaded(aNewWindow).then(function() {
        checkReferrerAndStartNextTest(aTestNumber, aNewWindow, null,
                                      startNewWindowTestCase,
                                      { userContextId: 1 });
      });
    });

    doContextMenuCommand(gTestWindow, aContextMenu, "context-openlink");
  });
}

function test() {
  requestLongerTimeout(10);  // slowwww shutdown on e10s
  startReferrerTest(startNewWindowTestCase, { userContextId: 1 });
}
