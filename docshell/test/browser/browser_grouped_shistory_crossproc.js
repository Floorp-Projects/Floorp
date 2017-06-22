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

  // Wait for the given tab being closed.
  function awaitTabClose(tab) {
    return new Promise(resolve => {
      tab.addEventListener("TabClose", function() {
        ok(true, "The tab is being closed!\n");
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

    // Load a URI which will involve loading in the parent process
    let tabClose = awaitTabClose(tab2);
    browser1.loadURI("about:config", Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
    await BrowserTestUtils.browserLoaded(browser1);
    let docshell = browser1.frameLoader.docShell.QueryInterface(Ci.nsIWebNavigation);
    ok(docshell, "The browser should be loaded in the chrome process");
    is(docshell.canGoForward, false, "canGoForward is correct");
    is(docshell.canGoBack, true, "canGoBack is correct");
    is(docshell.sessionHistory.count, 3, "Count is correct");
    is(browser1.frameLoader.groupedSHistory, null,
       "browser1's session history is now complete");
    await tabClose;
  });
});
