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

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html,a" }, function* (browser1) {
    // Set up the grouped SHEntry setup
    let tab2 = gBrowser.loadOneTab("data:text/html,b", {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
    });
    yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
    browser1.frameLoader.appendPartialSHistoryAndSwap(tab2.linkedBrowser.frameLoader);
    yield awaitProcessChange(browser1);

    // Close tab2 such that the back frameloader is dead
    yield BrowserTestUtils.removeTab(tab2);
    browser1.goBack();
    yield BrowserTestUtils.browserLoaded(browser1);
    yield ContentTask.spawn(browser1, null, function() {
      is(content.window.location + "", "data:text/html,a");
      let webNav = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebNavigation);
      is(webNav.canGoForward, true, "canGoForward is correct");
      is(webNav.canGoBack, false, "canGoBack is correct");
    });
    is(browser1.frameLoader.groupedSHistory, null,
       "browser1's session history is now complete");
  });
});
