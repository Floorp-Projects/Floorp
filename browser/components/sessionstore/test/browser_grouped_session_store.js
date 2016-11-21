add_task(function* () {
  const URIs = [
    "data:text/html,1",
    "data:text/html,2",
    "data:text/html,3",
    "data:text/html,4",
    "data:text/html,5",
  ];

  const {TabStateCache} = Cu.import("resource:///modules/sessionstore/TabStateCache.jsm", {});
  const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

  yield SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true]]
  });

  // Check that the data stored in the TabStateCache is correct for the current state.
  function* validate(browser, length, index) {
    yield TabStateFlusher.flush(browser);
    let {history} = TabStateCache.get(browser);
    is(history.entries.length, length, "Lengths match");
    for (let i = 0; i < length; ++i) {
      is(history.entries[i].url, URIs[i], "URI at index " + i + " matches");
    }
    is(history.index, index, "Index matches");
    yield ContentTask.spawn(browser, [index, length], function* ([index, length]) {
      let webNav = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebNavigation);
      is(webNav.canGoForward, index < length, "canGoForward is correct");
      is(webNav.canGoBack, index > 1, "canGoBack is correct");
    });
  }

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

  // Order of events:
  // Load [0], load [1], prerender [2], load [2], load [3]
  // Back [2], Back [1], Forward [2], Back [0], Forward [3]
  // Prerender [4], Back [0], Forward [2], Load [3'], Back [0].
  yield BrowserTestUtils.withNewTab({ gBrowser, url: URIs[0] }, function* (browser1) {
    yield* validate(browser1, 1, 1);

    browser1.loadURI(URIs[1], null, null);
    yield BrowserTestUtils.browserLoaded(browser1);
    yield* validate(browser1, 2, 2);

    // Create a new hidden prerendered tab to swap to.
    let tab2 = gBrowser.loadOneTab(URIs[2], {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
    });
    yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
    browser1.frameLoader.appendPartialSessionHistoryAndSwap(
      tab2.linkedBrowser.frameLoader);
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 3, 3);

    browser1.loadURI(URIs[3], null, null);
    yield BrowserTestUtils.browserLoaded(browser1);
    yield* validate(browser1, 4, 4);

    // In process navigate back.
    let p = BrowserTestUtils.waitForContentEvent(browser1, "pageshow");
    browser1.goBack();
    yield p;
    yield* validate(browser1, 4, 3);

    // Cross process navigate back.
    browser1.goBack();
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 4, 2);

    // Cross process navigate forward.
    browser1.goForward();
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 4, 3);

    // Navigate across process to a page which was not recently loaded.
    browser1.gotoIndex(0);
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 4, 1);

    // Navigate across process to a page which was not recently loaded in the other direction.
    browser1.gotoIndex(3);
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 4, 4);

    // Create a new hidden prerendered tab to swap to
    let tab3 = gBrowser.loadOneTab(URIs[4], {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_DEFAULT,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
    });
    yield BrowserTestUtils.browserLoaded(tab3.linkedBrowser);
    browser1.frameLoader.appendPartialSessionHistoryAndSwap(
      tab3.linkedBrowser.frameLoader);
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 5, 5);

    browser1.gotoIndex(0);
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 5, 1);

    browser1.gotoIndex(2);
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 5, 3);

    // Load a new page and make sure it throws out all of the following entries.
    URIs[3] = "data:text/html,NEW";
    browser1.loadURI(URIs[3]);
    yield BrowserTestUtils.browserLoaded(browser1);
    yield* validate(browser1, 4, 4);

    browser1.gotoIndex(0);
    yield awaitProcessChange(browser1);
    yield* validate(browser1, 4, 1);

    // XXX: This will be removed automatically by the owning tab closing in the
    // future, but this is not supported yet.
    gBrowser.removeTab(tab2);
    gBrowser.removeTab(tab3);
  });
});
