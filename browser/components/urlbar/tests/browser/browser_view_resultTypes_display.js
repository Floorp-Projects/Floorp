/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {SyncedTabs} = ChromeUtils.import("resource://services-sync/SyncedTabs.jsm");

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

function assertElementsDisplayed(details, expected) {
  Assert.equal(details.type, expected.type,
    "Should be displaying a row of the correct type");
  Assert.equal(details.title, expected.title,
    "Should be displaying the correct title");
  if (expected.separator) {
    Assert.notEqual(window.getComputedStyle(details.element.separator).display,
                    "none", "Should be displaying a separator");
    Assert.notEqual(window.getComputedStyle(details.element.separator).visibility,
                    "collapse", "Should be displaying a separator");
  } else {
    Assert.ok(window.getComputedStyle(details.element.separator).display == "none" ||
              window.getComputedStyle(details.element.separator).visibility == "collapse",
              "Should not be displaying a separator");
  }
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.urlbar.suggest.searches", false],
    // Disable search suggestions in the urlbar.
    ["browser.urlbar.suggest.searches", false],
    // Clear historical search suggestions to avoid interference from previous
    // tests.
    ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
    // Use the default matching bucket configuration.
    ["browser.urlbar.matchBuckets", "general:5,suggestion:4"],
    // Turn autofill off.
    ["browser.urlbar.autoFill", false],
    // Special prefs for remote tabs.
    ["services.sync.username", "fake"],
    ["services.sync.syncedTabs.showRemoteTabs", true],
  ]});

  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  registerCleanupFunction(async () => {
    await Services.search.setDefault(oldDefaultEngine);
  });
});

add_task(async function test_tab_switch_result() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  await BrowserTestUtils.withNewTab({gBrowser}, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "about:mozilla",
      fireInputEvent: true,
    });

    const details = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);

    assertElementsDisplayed(details, {
      separator: true,
      title: "about:mozilla",
      type: UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    });
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_search_result() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", true);

  await BrowserTestUtils.withNewTab({gBrowser}, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "foo",
      fireInputEvent: true,
    });

    const details = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);

    // We'll initially display no separator.
    assertElementsDisplayed(details, {
      separator: false,
      title: "foofoo",
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
    });

    EventUtils.sendKey("down");

    // We should now be displaying one.
    assertElementsDisplayed(details, {
      separator: true,
      title: "foofoo",
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
    });
  });

  await PlacesUtils.history.clear();
  Services.prefs.setBoolPref("browser.urlbar.suggest.searches", false);
});

add_task(async function test_url_result() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com",
    title: "example",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);

  await BrowserTestUtils.withNewTab({gBrowser}, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "example",
      fireInputEvent: true,
    });

    const details = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);

    assertElementsDisplayed(details, {
      separator: true,
      title: "example",
      type: UrlbarUtils.RESULT_TYPE.URL,
    });
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_keyword_result() {
  const TEST_URL = `${TEST_BASE_URL}print_postdata.sjs`;

  await PlacesUtils.keywords.insert({
    keyword: "get",
    url: TEST_URL + "?q=%s",
  });

  await BrowserTestUtils.withNewTab({gBrowser}, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "get ",
      fireInputEvent: true,
    });

    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

    assertElementsDisplayed(details, {
      separator: true,
      title: "example.com",
      type: UrlbarUtils.RESULT_TYPE.KEYWORD,
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "get test",
      fireInputEvent: true,
    });

    details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

    assertElementsDisplayed(details, {
      separator: false,
      title: "example.com: test",
      type: UrlbarUtils.RESULT_TYPE.KEYWORD,
    });
  });
});

add_task(async function test_omnibox_result() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "omnibox": {
        "keyword": "omniboxtest",
      },

      background() {
        /* global browser */
        browser.omnibox.setDefaultSuggestion({
          description: "doit",
        });
        // Just do nothing for this test.
        browser.omnibox.onInputEntered.addListener(() => {
        });
        browser.omnibox.onInputChanged.addListener((text, suggest) => {
          suggest([]);
        });
      },
    },
  });

  await extension.startup();

  await BrowserTestUtils.withNewTab({gBrowser}, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "omniboxtest ",
      fireInputEvent: true,
    });

    const details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

    assertElementsDisplayed(details, {
      separator: true,
      title: "Generated extension",
      type: UrlbarUtils.RESULT_TYPE.OMNIBOX,
    });
  });

  await extension.unload();
});

add_task(async function test_remote_tab_result() {
  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  const REMOTE_TAB = {
    "id": "7cqCr77ptzX3",
    "type": "client",
    "lastModified": 1492201200,
    "name": "zcarter's Nightly on MacBook-Pro-25",
    "clientType": "desktop",
    "tabs": [
      {
        "type": "tab",
        "title": "Test Remote",
        "url": "http://example.com",
        "icon": UrlbarUtils.ICON.DEFAULT,
        "client": "7cqCr77ptzX3",
        "lastUsed": 1452124677,
      },
    ],
  };

  const sandbox = sinon.createSandbox();

  let originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() { return Promise.resolve([]); },
    syncTabs() { return Promise.resolve(); },
  };

  // Tell the Sync XPCOM service it is initialized.
  let weaveXPCService = Cc["@mozilla.org/weave/service;1"]
                          .getService(Ci.nsISupports)
                          .wrappedJSObject;
  let oldWeaveServiceReady = weaveXPCService.ready;
  weaveXPCService.ready = true;

  sandbox.stub(SyncedTabs._internal, "getTabClients")
         .callsFake(() => Promise.resolve(Cu.cloneInto([REMOTE_TAB], {})));

  registerCleanupFunction(async function() {
    sandbox.restore();
    weaveXPCService.ready = oldWeaveServiceReady;
    SyncedTabs._internal = originalSyncedTabsInternal;
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
  });

  await BrowserTestUtils.withNewTab({gBrowser}, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus,
      value: "example",
      fireInputEvent: true,
    });

    const details = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);

    assertElementsDisplayed(details, {
      separator: true,
      title: "Test Remote",
      type: UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
    });
  });
});
