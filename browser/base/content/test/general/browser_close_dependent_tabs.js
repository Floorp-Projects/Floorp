add_task(function* () {
  yield SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true]]
  });

  // Wait for a process change and then fulfil the promise.
  function awaitProcessChange(browser) {
    return new Promise(resolve => {
      browser.addEventListener("BrowserChangedProcess", function bcp(e) {
        browser.removeEventListener("BrowserChangedProcess", bcp);
        ok(true, "The browser changed process!");
        resolve();
      });
    });
  }

  let tab2;

  // Test 1: Create prerendered browser, and don't switch to it, then close the tab
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html,a" }, function* (browser1) {
    // Set up the grouped SHEntry setup
    tab2 = gBrowser.loadOneTab("data:text/html,b", {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
    });
  });

  // At this point tab2 should be closed
  todo(!tab2.linkedBrowser, "The new tab should be closed");
  yield BrowserTestUtils.removeTab(tab2); // XXX: Shouldn't be needed once the todo is fixed

  // Test 2: Create prerendered browser, switch to it, then close the tab
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html,a" }, function* (browser1) {
    // Set up the grouped SHEntry setup
    tab2 = gBrowser.loadOneTab("data:text/html,b", {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
    });
    yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
    browser1.frameLoader.appendPartialSessionHistoryAndSwap(
      tab2.linkedBrowser.frameLoader);
    yield awaitProcessChange(browser1);
  });

  // At this point tab2 should be closed
  ok(!tab2.linkedBrowser, "The new tab should be closed");
});
