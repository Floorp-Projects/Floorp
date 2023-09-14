add_task(async function () {
  // The urls don't really matter as long as they are of the same origin
  var URL =
    "http://mochi.test:8888/browser/docshell/test/navigation/bug343515_pg1.html";
  var URL2 =
    "http://mochi.test:8888/browser/docshell/test/navigation/bug343515_pg3_1.html";

  // We want to test a specific code path that leads to this call
  // https://searchfox.org/mozilla-central/rev/e7c61f4a68b974d5fecd216dc7407b631a24eb8f/docshell/base/nsDocShell.cpp#10795
  // when gHistoryMaxSize is 0 and mIndex and mRequestedIndex are -1

  // 1. Navigate to URL
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      // At this point, we haven't set gHistoryMaxSize to 0, and it is still 50 (default value).
      if (SpecialPowers.Services.appinfo.sessionHistoryInParent) {
        let sh = browser.browsingContext.sessionHistory;
        is(
          sh.count,
          1,
          "We should have entry in session history because we haven't changed gHistoryMaxSize to be 0 yet"
        );
        is(
          sh.index,
          0,
          "Shistory's current index should be 0 because we haven't purged history yet"
        );
      } else {
        await ContentTask.spawn(browser, null, () => {
          var sh = content.window.docShell.QueryInterface(Ci.nsIWebNavigation)
            .sessionHistory.legacySHistory;
          is(
            sh.count,
            1,
            "We should have entry in session history because we haven't changed gHistoryMaxSize to be 0 yet"
          );
          is(
            sh.index,
            0,
            "Shistory's current index should be 0 because we haven't purged history yet"
          );
        });
      }

      var loadPromise = BrowserTestUtils.browserLoaded(browser, false, URL2);
      // If we set the pref at the beginning of this page, then when we launch a child process
      // to navigate to URL in Step 1, because of
      // https://searchfox.org/mozilla-central/rev/e7c61f4a68b974d5fecd216dc7407b631a24eb8f/docshell/shistory/nsSHistory.cpp#308-312
      // this pref will be set to the default value (currently 50). Setting this pref after the child process launches
      // is a robust way to make sure it stays 0
      await SpecialPowers.pushPrefEnv({
        set: [["browser.sessionhistory.max_entries", 0]],
      });
      // 2. Navigate to URL2
      // We are navigating to a page with the same origin so that we will stay in the same process
      BrowserTestUtils.startLoadingURIString(browser, URL2);
      await loadPromise;

      // 3. Reload the browser with specific flags so that we end up here
      // https://searchfox.org/mozilla-central/rev/e7c61f4a68b974d5fecd216dc7407b631a24eb8f/docshell/base/nsDocShell.cpp#10795
      var promise = BrowserTestUtils.browserLoaded(browser);
      browser.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
      await promise;

      if (SpecialPowers.Services.appinfo.sessionHistoryInParent) {
        let sh = browser.browsingContext.sessionHistory;
        is(sh.count, 0, "We should not save any entries in session history");
        is(sh.index, -1);
        is(sh.requestedIndex, -1);
      } else {
        await ContentTask.spawn(browser, null, () => {
          var sh = content.window.docShell.QueryInterface(Ci.nsIWebNavigation)
            .sessionHistory.legacySHistory;
          is(sh.count, 0, "We should not save any entries in session history");
          is(sh.index, -1);
          is(sh.requestedIndex, -1);
        });
      }
    }
  );
});
