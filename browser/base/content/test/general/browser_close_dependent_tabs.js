add_task(function* () {
  yield SpecialPowers.pushPrefEnv({
    set: [["browser.groupedhistory.enabled", true],
          ["dom.linkPrerender.enabled", true]]
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

  // Wait for given number tabs being closed.
  function awaitTabClose(number) {
    return new Promise(resolve => {
      let seen = 0;
      gBrowser.tabContainer.addEventListener("TabClose", function f() {
        if (++seen == number) {
          gBrowser.tabContainer.removeEventListener("TabClose", f);
          resolve();
        }
      });
    });
  }

  // Test 1: Create prerendered browser, and don't switch to it, then close the tab
  let closed1 = awaitTabClose(2);
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html,a" }, function* (browser1) {
    // Set up the grouped SHEntry setup

    let requestMade = new Promise(resolve => {
      browser1.messageManager.addMessageListener("Prerender:Request", function f() {
        browser1.messageManager.removeMessageListener("Prerender:Request", f);
        ok(true, "Successfully received the prerender request");
        resolve();
      });
    });

    is(gBrowser.tabs.length, 2);
    yield ContentTask.spawn(browser1, null, function() {
      let link = content.document.createElement("link");
      link.setAttribute("rel", "prerender");
      link.setAttribute("href", "data:text/html,b");
      content.document.body.appendChild(link);
    });
    yield requestMade;

    is(gBrowser.tabs.length, 3);
  });
  yield closed1;

  // At this point prerendered tab should be closed
  is(gBrowser.tabs.length, 1, "The new tab and the prerendered 'tab' should be closed");

  // Test 2: Create prerendered browser, switch to it, then close the tab
  let closed2 = awaitTabClose(2);
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "data:text/html,a" }, function* (browser1) {
    // Set up the grouped SHEntry setup
    let tab2 = gBrowser.loadOneTab("data:text/html,b", {
      referrerPolicy: Ci.nsIHttpChannel.REFERRER_POLICY_UNSET,
      allowThirdPartyFixup: true,
      relatedToCurrent: true,
      isPrerendered: true,
    });
    yield BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
    browser1.frameLoader.appendPartialSHistoryAndSwap(tab2.linkedBrowser.frameLoader);
    yield awaitProcessChange(browser1);
  });
  yield closed2;

  // At this point prerendered tab should be closed
  is(gBrowser.tabs.length, 1, "The new tab and the prerendered 'tab' should be closed");
});
