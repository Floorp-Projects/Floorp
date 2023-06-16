/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that results with hostnames other than the search mode engine are not
 * shown.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
});

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", false],
      ["browser.urlbar.autoFill", false],
      // Special prefs for remote tabs.
      ["services.sync.username", "fake"],
      ["services.sync.syncedTabs.showRemoteTabs", true],
    ],
  });

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  // Note that the result domain is subdomain.example.ca. We still expect to
  // match with example.com results because we ignore subdomains and the public
  // suffix in this check.
  await SearchTestUtils.installSearchExtension(
    {
      search_url: "https://subdomain.example.ca/",
    },
    { setAsDefault: true }
  );
  let engine = Services.search.getEngineByName("Example");
  await Services.search.moveEngine(engine, 0);

  const REMOTE_TAB = {
    id: "7cqCr77ptzX3",
    type: "client",
    lastModified: 1492201200,
    name: "Nightly on MacBook-Pro",
    clientType: "desktop",
    tabs: [
      {
        type: "tab",
        title: "Test Remote",
        url: "https://example.com",
        icon: UrlbarUtils.ICON.DEFAULT,
        client: "7cqCr77ptzX3",
        lastUsed: Math.floor(Date.now() / 1000),
      },
      {
        type: "tab",
        title: "Test Remote 2",
        url: "https://example-2.com",
        icon: UrlbarUtils.ICON.DEFAULT,
        client: "7cqCr77ptzX3",
        lastUsed: Math.floor(Date.now() / 1000),
      },
    ],
  };

  const sandbox = sinon.createSandbox();

  let originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() {
      return Promise.resolve([]);
    },
    syncTabs() {
      return Promise.resolve();
    },
  };

  // Tell the Sync XPCOM service it is initialized.
  let weaveXPCService = Cc["@mozilla.org/weave/service;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
  let oldWeaveServiceReady = weaveXPCService.ready;
  weaveXPCService.ready = true;

  sandbox
    .stub(SyncedTabs._internal, "getTabClients")
    .callsFake(() => Promise.resolve(Cu.cloneInto([REMOTE_TAB], {})));

  // Reset internal cache in UrlbarProviderRemoteTabs.
  Services.obs.notifyObservers(null, "weave:engine:sync:finish", "tabs");

  registerCleanupFunction(async function () {
    sandbox.restore();
    weaveXPCService.ready = oldWeaveServiceReady;
    SyncedTabs._internal = originalSyncedTabsInternal;
    await PlacesUtils.history.clear();
  });
});

add_task(async function basic() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    3,
    "We have three results"
  );
  let firstResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    firstResult.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "The first result is the heuristic search result."
  );
  let secondResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
    "The second result is a remote tab."
  );
  let thirdResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    thirdResult.type,
    UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
    "The third result is a remote tab."
  );

  await UrlbarTestUtils.enterSearchMode(window);

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "We have two results. The second remote tab result is excluded despite matching the search string."
  );
  firstResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    firstResult.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "The first result is the heuristic search result."
  );
  secondResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
    "The second result is a remote tab."
  );

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});

// For engines with an invalid TLD, we filter on the entire domain.
add_task(async function malformedEngine() {
  await SearchTestUtils.installSearchExtension({
    name: "TestMalformed",
    search_url: "https://example.foobar/",
  });
  let badEngine = Services.search.getEngineByName("TestMalformed");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "example",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    4,
    "We have four results"
  );
  let firstResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    firstResult.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "The first result is the heuristic search result."
  );
  let secondResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    secondResult.type,
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    "The second result is the tab-to-search onboarding result for our malformed engine."
  );
  let thirdResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(
    thirdResult.type,
    UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
    "The third result is a remote tab."
  );
  let fourthResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 3);
  Assert.equal(
    fourthResult.type,
    UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
    "The fourth result is a remote tab."
  );

  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: badEngine.name,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "We only have one result."
  );
  firstResult = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(firstResult.heuristic, "The first result is heuristic.");
  Assert.equal(
    firstResult.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "The first result is the heuristic search result."
  );

  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
});
