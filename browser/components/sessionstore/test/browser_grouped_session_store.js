add_task(async function() {
  const URIs = [
    "data:text/html,1",
    "data:text/html,2",
    "data:text/html,3",
    "data:text/html,4",
    "data:text/html,5",
  ];

  const {TabStateCache} = Cu.import("resource:///modules/sessionstore/TabStateCache.jsm", {});
  const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

  await SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true]]
  });

  // Check that the data stored in the TabStateCache is correct for the current state.
  async function validate(browser, length, index) {
    await TabStateFlusher.flush(browser);
    let {history} = TabStateCache.get(browser);
    is(history.entries.length, length, "Lengths match");
    for (let i = 0; i < length; ++i) {
      is(history.entries[i].url, URIs[i], "URI at index " + i + " matches");
    }
    is(history.index, index, "Index matches");
    await ContentTask.spawn(browser, [index, length], async function([expectedIndex, expectedLength]) {
      let webNav = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebNavigation);
      is(webNav.sessionHistory.globalIndexOffset + webNav.sessionHistory.index,
         expectedIndex - 1, "In content index matches");
      is(webNav.canGoForward, expectedIndex < expectedLength, "canGoForward is correct");
      is(webNav.canGoBack, expectedIndex > 1, "canGoBack is correct");
    });
  }

  // Wait for a process change, followed by a locationchange event, and then
  // fulfil the promise.
  function awaitProcessChange(browser) {
    return new Promise(resolve => {
      let locChangeListener = {
        onLocationChange: () => {
          gBrowser.removeProgressListener(locChangeListener);
          resolve();
        },
      };

      browser.addEventListener("BrowserChangedProcess", function(e) {
        gBrowser.addProgressListener(locChangeListener);
      }, {once: true});
    });
  }

  // Order of events:
  // Load [0], load [1], prerender [2], load [2], load [3]
  // Back [2], Back [1], Forward [2], Back [0], Forward [3]
  // Prerender [4], Back [0], Forward [2], Load [3'], Back [0].
  await BrowserTestUtils.withNewTab({ gBrowser, url: URIs[0] }, async function(browser1) {
    await validate(browser1, 1, 1);

    browser1.loadURI(URIs[1], null, null);
    await BrowserTestUtils.browserLoaded(browser1);
    await validate(browser1, 2, 2);

    // Create a new hidden prerendered tab to swap to.
    let tab2 = gBrowser.loadOneTab(URIs[2], {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
    browser1.frameLoader.appendPartialSHistoryAndSwap(tab2.linkedBrowser.frameLoader);
    await awaitProcessChange(browser1);
    await validate(browser1, 3, 3);

    browser1.loadURI(URIs[3], null, null);
    await BrowserTestUtils.browserLoaded(browser1);
    await validate(browser1, 4, 4);

    // In process navigate back.
    let p = BrowserTestUtils.waitForContentEvent(browser1, "pageshow");
    browser1.goBack();
    await p;
    await validate(browser1, 4, 3);

    // Cross process navigate back.
    browser1.goBack();
    await awaitProcessChange(browser1);
    await validate(browser1, 4, 2);

    // Cross process navigate forward.
    browser1.goForward();
    await awaitProcessChange(browser1);
    await validate(browser1, 4, 3);

    // Navigate across process to a page which was not recently loaded.
    browser1.gotoIndex(0);
    await awaitProcessChange(browser1);
    await validate(browser1, 4, 1);

    // Navigate across process to a page which was not recently loaded in the other direction.
    browser1.gotoIndex(3);
    await awaitProcessChange(browser1);
    await validate(browser1, 4, 4);

    // Create a new hidden prerendered tab to swap to
    let tab3 = gBrowser.loadOneTab(URIs[4], {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    await BrowserTestUtils.browserLoaded(tab3.linkedBrowser);
    browser1.frameLoader.appendPartialSHistoryAndSwap(tab3.linkedBrowser.frameLoader);
    await awaitProcessChange(browser1);
    await validate(browser1, 5, 5);

    browser1.gotoIndex(0);
    await awaitProcessChange(browser1);
    await validate(browser1, 5, 1);

    browser1.gotoIndex(2);
    await awaitProcessChange(browser1);
    await validate(browser1, 5, 3);

    // Load a new page and make sure it throws out all of the following entries.
    URIs[3] = "data:text/html,NEW";
    browser1.loadURI(URIs[3]);
    await BrowserTestUtils.browserLoaded(browser1);
    await validate(browser1, 4, 4);

    browser1.gotoIndex(0);
    await awaitProcessChange(browser1);
    await validate(browser1, 4, 1);

    // XXX: This will be removed automatically by the owning tab closing in the
    // future, but this is not supported yet.
    gBrowser.removeTab(tab2);
    gBrowser.removeTab(tab3);
  });
});
