// Tests referrer on middle-click navigation.
// Middle-clicks on the link, which opens it in a new tab, same container.

function startMiddleClickTestCase(aTestNumber) {
  info(
    "browser_referrer_middle_click: " + getReferrerTestDescription(aTestNumber)
  );
  someTabLoaded(gTestWindow).then(function(aNewTab) {
    BrowserTestUtils.switchTab(gTestWindow.gBrowser, aNewTab).then(() => {
      checkReferrerAndStartNextTest(
        aTestNumber,
        null,
        aNewTab,
        startMiddleClickTestCase,
        { userContextId: 3 }
      );
    });
  });

  clickTheLink(gTestWindow, "testlink", { button: 1 });
}

function test() {
  waitForExplicitFinish();

  SpecialPowers.pushPrefEnv(
    { set: [["privacy.userContext.enabled", true]] },
    function() {
      requestLongerTimeout(10); // slowwww shutdown on e10s
      startReferrerTest(startMiddleClickTestCase, { userContextId: 3 });
    }
  );
}
