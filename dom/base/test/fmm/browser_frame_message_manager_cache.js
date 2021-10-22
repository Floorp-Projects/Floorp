add_task(async function testCacheAfterInvalidate() {
  // Load some page to make scripts cached.
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:addons"
  );

  // Discard ScriptPreloader cache.
  Services.obs.notifyObservers(null, "startupcache-invalidate");

  // Load some other page to use the cache in nsMessageManagerScriptExecutor
  // cache.
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  // Verify the browser doesn't crash.
  ok(true);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
