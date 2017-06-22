add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true]]
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

  function isAlive(tab) {
    return tab.linkedBrowser &&
      tab.linkedBrowser.frameLoader &&
      !tab.linkedBrowser.frameLoader.isDead;
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
    ok(isAlive(tab2));

    // Load some URIs and make sure that we lose the old process once we are 3 history entries away.
    browser1.loadURI("data:text/html,c", null, null);
    await BrowserTestUtils.browserLoaded(browser1);
    ok(isAlive(tab2), "frameloader should still be alive");
    browser1.loadURI("data:text/html,d", null, null);
    await BrowserTestUtils.browserLoaded(browser1);
    ok(isAlive(tab2), "frameloader should still be alive");
    browser1.loadURI("data:text/html,e", null, null);
    await BrowserTestUtils.browserLoaded(browser1);
    ok(isAlive(tab2), "frameloader should still be alive");

    // The 4th navigation should kill the frameloader
    browser1.loadURI("data:text/html,f", null, null);
    await new Promise(resolve => {
      tab2.addEventListener("TabClose", function() {
        ok(true, "The tab is being closed!\n");
        resolve();
      }, {once: true});
    });
    // We don't check for !isAlive() as TabClose is called during
    // _beginRemoveTab, which means that the frameloader may not be dead yet. We
    // avoid races by not checking.
  });
});
