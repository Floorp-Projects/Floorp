/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that results with hostnames other than the search mode engine are not
 * shown.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

add_task(async function setup() {
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

  let oldDefaultEngine = await Services.search.getDefault();
  // Note that the result domain is subdomain.example.ca. We still expect to
  // match with example.com results because we ignore subdomains and the public
  // suffix in this check.
  let engine = await Services.search.addEngineWithDetails("Test", {
    template: `http://subdomain.example.ca/?search={searchTerms}`,
  });
  await Services.search.setDefault(engine);
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
        url: "http://example.com",
        icon: UrlbarUtils.ICON.DEFAULT,
        client: "7cqCr77ptzX3",
        lastUsed: 1452124677,
      },
      {
        type: "tab",
        title: "Test Remote 2",
        url: "http://example-2.com",
        icon: UrlbarUtils.ICON.DEFAULT,
        client: "7cqCr77ptzX3",
        lastUsed: 1452124677,
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

  // Reset internal cache in PlacesRemoteTabsAutocompleteProvider.
  Services.obs.notifyObservers(null, "weave:engine:sync:finish", "tabs");

  registerCleanupFunction(async function() {
    sandbox.restore();
    weaveXPCService.ready = oldWeaveServiceReady;
    SyncedTabs._internal = originalSyncedTabsInternal;
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
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
  let badEngine = await Services.search.addEngineWithDetails("TestMalformed", {
    template: `http://example.foobar/?search={searchTerms}`,
  });

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
  await Services.search.removeEngine(badEngine);
});
