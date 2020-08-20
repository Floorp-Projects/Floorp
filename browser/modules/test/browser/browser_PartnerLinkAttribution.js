/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with search related actions.
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

// The preference to enable suggestions in the urlbar.
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
// The name of the search engine used to generate suggestions.
const SUGGESTION_ENGINE_NAME =
  "browser_UsageTelemetry usageTelemetrySearchSuggestions.xml";

XPCOMUtils.defineLazyModuleGetters(this, {
  CustomizableUITestUtils:
    "resource://testing-common/CustomizableUITestUtils.jsm",
  Region: "resource://gre/modules/Region.jsm",
  SearchTelemetry: "resource:///modules/SearchTelemetry.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  HttpServer: "resource://testing-common/httpd.js",
});

let gCUITestUtils = new CustomizableUITestUtils(window);
SearchTestUtils.init(Assert, registerCleanupFunction);

var gHttpServer = null;
var gRequests = [];

function submitHandler(request, response) {
  gRequests.push(request);
  response.setStatusLine(request.httpVersion, 200, "Ok");
}

add_task(async function setup() {
  // Ensure the initial init is complete.
  await Services.search.init();

  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();

  let searchExtensions = getChromeDir(getResolvedURI(gTestPath));
  searchExtensions.append("search-engines");

  await SearchTestUtils.useMochitestEngines(searchExtensions);

  SearchTestUtils.useMockIdleService();
  let response = await fetch(`resource://search-extensions/engines.json`);
  let json = await response.json();
  await SearchTestUtils.updateRemoteSettingsConfig(json.data);

  gHttpServer = new HttpServer();
  gHttpServer.registerPathHandler("/", submitHandler);
  gHttpServer.start(-1);

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable search suggestions in the urlbar.
      [SUGGEST_URLBAR_PREF, true],
      // Clear historical search suggestions to avoid interference from previous
      // tests.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
      // Use the default matching bucket configuration.
      ["browser.urlbar.matchBuckets", "general:5,suggestion:4"],
      //
      [
        "browser.partnerlink.attributionURL",
        `http://localhost:${gHttpServer.identity.primaryPort}/`,
      ],
    ],
  });

  await gCUITestUtils.addSearchBar();

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    await SearchTestUtils.updateRemoteSettingsConfig();
    await gHttpServer.stop();
    gHttpServer = null;
    await PlacesUtils.history.clear();
    gCUITestUtils.removeSearchBar();
  });
});

function searchInAwesomebar(value, win = window) {
  return UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    waitForFocus,
    value,
    fireInputEvent: true,
  });
}

add_task(async function test_simpleQuery_no_attribution() {
  await Services.search.setDefault(
    Services.search.getEngineByName("Simple Engine")
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Simulate entering a simple search.");
  let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    "https://example.com/?sourceId=Mozilla-search&search=simple+query",
    tab
  );
  await searchInAwesomebar("simple query");
  EventUtils.synthesizeKey("KEY_Enter");
  await promiseLoad;

  await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));

  Assert.equal(gRequests.length, 0, "Should not have submitted an attribution");

  BrowserTestUtils.removeTab(tab);

  await Services.search.setDefault(Services.search.getEngineByName("basic"));
});

async function checkAttributionRecorded(actionFn, cleanupFn) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/plain;charset=utf8,simple%20query"
  );

  let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    "https://mochi.test:8888/browser/browser/components/search/test/browser/?search=simple+query&foo=1",
    tab
  );
  await actionFn(tab);
  await promiseLoad;

  await BrowserTestUtils.waitForCondition(
    () => gRequests.length == 1,
    "Should have received an attribution submission"
  );
  Assert.equal(
    gRequests[0].getHeader("x-region"),
    Region.home,
    "Should have set the region correctly"
  );
  Assert.equal(
    gRequests[0].getHeader("X-Source"),
    "searchurl",
    "Should have set the source correctly"
  );
  Assert.equal(
    gRequests[0].getHeader("X-Target-url"),
    "https://mochi.test:8888/browser/browser/components/search/test/browser/?foo=1",
    "Should have set the target url correctly and stripped the search terms"
  );
  if (cleanupFn) {
    await cleanupFn();
  }
  BrowserTestUtils.removeTab(tab);
  gRequests = [];
}

add_task(async function test_urlbar() {
  await checkAttributionRecorded(async tab => {
    await searchInAwesomebar("simple query");
    EventUtils.synthesizeKey("KEY_Enter");
  });
});

add_task(async function test_searchbar() {
  await checkAttributionRecorded(async tab => {
    let sb = BrowserSearch.searchBar;
    // Write the search query in the searchbar.
    sb.focus();
    sb.value = "simple query";
    sb.textbox.controller.startSearch("simple query");
    // Wait for the popup to show.
    await BrowserTestUtils.waitForEvent(sb.textbox.popup, "popupshown");
    // And then for the search to complete.
    await BrowserTestUtils.waitForCondition(
      () =>
        sb.textbox.controller.searchStatus >=
        Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
      "The search in the searchbar must complete."
    );
    EventUtils.synthesizeKey("KEY_Enter");
  });
});

add_task(async function test_context_menu() {
  let contextMenu;
  await checkAttributionRecorded(
    async tab => {
      info("Select all the text in the page.");
      await SpecialPowers.spawn(tab.linkedBrowser, [""], async function() {
        return new Promise(resolve => {
          content.document.addEventListener(
            "selectionchange",
            () => resolve(),
            {
              once: true,
            }
          );
          content.document
            .getSelection()
            .selectAllChildren(content.document.body);
        });
      });

      info("Open the context menu.");
      contextMenu = document.getElementById("contentAreaContextMenu");
      let popupPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      BrowserTestUtils.synthesizeMouseAtCenter(
        "body",
        { type: "contextmenu", button: 2 },
        gBrowser.selectedBrowser
      );
      await popupPromise;

      info("Click on search.");
      let searchItem = contextMenu.getElementsByAttribute(
        "id",
        "context-searchselect"
      )[0];
      searchItem.click();
    },
    () => {
      contextMenu.hidePopup();
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  );
});

add_task(async function test_about_newtab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(() => !content.document.hidden);
  });

  info("Trigger a simple serch, just text + enter.");
  let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    "https://mochi.test:8888/browser/browser/components/search/test/browser/?search=simple+query&foo=1",
    tab
  );
  await typeInSearchField(
    tab.linkedBrowser,
    "simple query",
    "newtab-search-text"
  );
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  await promiseLoad;

  await BrowserTestUtils.waitForCondition(
    () => gRequests.length == 1,
    "Should have received an attribution submission"
  );
  Assert.equal(
    gRequests[0].getHeader("x-region"),
    Region.home,
    "Should have set the region correctly"
  );
  Assert.equal(
    gRequests[0].getHeader("X-Source"),
    "searchurl",
    "Should have set the source correctly"
  );
  Assert.equal(
    gRequests[0].getHeader("X-Target-url"),
    "https://mochi.test:8888/browser/browser/components/search/test/browser/?foo=1",
    "Should have set the target url correctly and stripped the search terms"
  );

  BrowserTestUtils.removeTab(tab);
  gRequests = [];
});
