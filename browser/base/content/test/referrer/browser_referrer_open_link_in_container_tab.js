// Tests referrer on context menu navigation - open link in new container tab.
// Selects "open link in new container tab" from the context menu.

function getReferrerTest(aTestNumber) {
  let testCase = _referrerTests[aTestNumber];
  if (testCase) {
    // We want all the referrer tests to fail!
    testCase.result = "";
  }

  return testCase;
}

function startNewTabTestCase(aTestNumber) {
  info("browser_referrer_open_link_in_container_tab: " +
       getReferrerTestDescription(aTestNumber));
  contextMenuOpened(gTestWindow, "testlink").then(function(aContextMenu) {
    someTabLoaded(gTestWindow).then(function(aNewTab) {
      gTestWindow.gBrowser.selectedTab = aNewTab;

      checkReferrerAndStartNextTest(aTestNumber, null, aNewTab,
                                    startNewTabTestCase);
    });

    let menu = gTestWindow.document.getElementById("context-openlinkinusercontext-menu");

    let menupopup = menu.menupopup;
    menu.addEventListener("popupshown", function onPopupShown() {
      menu.removeEventListener("popupshown", onPopupShown);

      is(menupopup.nodeType, Node.ELEMENT_NODE, "We have a menupopup.");
      ok(menupopup.firstChild, "We have a first container entry.");

      let firstContext = menupopup.firstChild;
      is(firstContext.nodeType, Node.ELEMENT_NODE, "We have a first container entry.");
      ok(firstContext.hasAttribute("data-usercontextid"), "We have a usercontextid value.");

      aContextMenu.addEventListener("popuphidden", function onPopupHidden() {
        aContextMenu.removeEventListener("popuphidden", onPopupHidden);
        firstContext.doCommand();
      });

      aContextMenu.hidePopup();
    });

    menupopup.showPopup();
  });
}

function test() {
  waitForExplicitFinish();

  SpecialPowers.pushPrefEnv(
    {set: [["privacy.userContext.enabled", true]]},
    function() {
      requestLongerTimeout(10);  // slowwww shutdown on e10s
      startReferrerTest(startNewTabTestCase);
    });
}
