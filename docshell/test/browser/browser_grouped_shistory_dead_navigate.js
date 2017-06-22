add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true],
          ["dom.ipc.processCount", 1]]
  });

  // Wait for a process change and then fulfil the promise.
  function awaitProcessChange(browser) {
    return new Promise(resolve => {
      browser.addEventListener("BrowserChangedProcess", function(e) {
        ok(true, "The browser changed process!");
        resolve();
      }, {once: true});
    });
  }

  await BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html,a" }, async function(browser1) {
    // Set up the grouped SHEntry setup
    let tab2 = gBrowser.loadOneTab("data:text/html,b", {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
    browser1.frameLoader.appendPartialSHistoryAndSwap(tab2.linkedBrowser.frameLoader);
    await awaitProcessChange(browser1);

    // Close tab2 such that the back frameloader is dead
    await BrowserTestUtils.removeTab(tab2);
    await BrowserTestUtils.waitForCondition(() => browser1.canGoBack);
    browser1.goBack();
    await BrowserTestUtils.browserLoaded(browser1);
    await ContentTask.spawn(browser1, null, function() {
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
