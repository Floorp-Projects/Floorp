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

ChromeUtils.defineESModuleGetters(this, {
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  CustomizableUITestUtils:
    "resource://testing-common/CustomizableUITestUtils.jsm",
  Region: "resource://gre/modules/Region.jsm",
  HttpServer: "resource://testing-common/httpd.js",
});

let gCUITestUtils = new CustomizableUITestUtils(window);
SearchTestUtils.init(this);

var gHttpServer = null;
var gRequests = [];

function submitHandler(request, response) {
  gRequests.push(request);
  response.setStatusLine(request.httpVersion, 200, "Ok");
}

add_setup(async function() {
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

  let topsitesAttribution = Services.prefs.getStringPref(
    "browser.partnerlink.campaign.topsites"
  );
  gHttpServer = new HttpServer();
  gHttpServer.registerPathHandler(`/cid/${topsitesAttribution}`, submitHandler);
  gHttpServer.start(-1);

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable search suggestions in the urlbar.
      [SUGGEST_URLBAR_PREF, true],
      // Clear historical search suggestions to avoid interference from previous
      // tests.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
      [
        "browser.partnerlink.attributionURL",
        `http://localhost:${gHttpServer.identity.primaryPort}/cid/`,
      ],
    ],
  });

  await gCUITestUtils.addSearchBar();

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    let settingsWritten = SearchTestUtils.promiseSearchNotification(
      "write-settings-to-disk-complete"
    );
    await SearchTestUtils.updateRemoteSettingsConfig();
    await gHttpServer.stop();
    gHttpServer = null;
    await PlacesUtils.history.clear();
    gCUITestUtils.removeSearchBar();
    await settingsWritten;
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

async function searchInSearchbar(inputText) {
  let win = window;
  await new Promise(r => waitForFocus(r, win));
  let sb = BrowserSearch.searchBar;
  // Write the search query in the searchbar.
  sb.focus();
  sb.value = inputText;
  sb.textbox.controller.startSearch(inputText);
  // Wait for the popup to be shown and built.
  let popup = sb.textbox.popup;
  await Promise.all([
    BrowserTestUtils.waitForEvent(popup, "popupshown"),
    BrowserTestUtils.waitForEvent(popup.oneOffButtons, "rebuild"),
  ]);
  // And then for the search to complete.
  await BrowserTestUtils.waitForCondition(
    () =>
      sb.textbox.controller.searchStatus >=
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
    "The search in the searchbar must complete."
  );
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
    gRequests[0].getHeader("X-Region"),
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
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
        false,
      ],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(() => !content.document.hidden);
  });

  info("Trigger a simple search, just text + enter.");
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
    gRequests[0].getHeader("X-Region"),
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
  await SpecialPowers.popPrefEnv();
  gRequests = [];
});

add_task(async function test_urlbar_oneOff_click() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Type a query.");
  let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    "https://mochi.test:8888/browser/browser/components/search/test/browser/?search=query&foo=1",
    tab
  );
  await searchInAwesomebar("query");
  info("Click the first one-off button.");
  UrlbarTestUtils.getOneOffSearchButtons(window)
    .getSelectableButtons(false)[0]
    .click();
  EventUtils.synthesizeKey("KEY_Enter");
  await promiseLoad;

  await BrowserTestUtils.waitForCondition(
    () => gRequests.length == 1,
    "Should have received an attribution submission"
  );
  Assert.equal(
    gRequests[0].getHeader("X-Region"),
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

add_task(async function test_searchbar_oneOff_click() {
  // For this test, set the other engine as default, so that we can select
  // the attribution engine as the first one in the one-offs.
  await Services.search.setDefault(
    Services.search.getEngineByName("Simple Engine")
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Type a query.");
  let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    "https://mochi.test:8888/browser/browser/components/search/test/browser/?search=searchbar&foo=1",
    tab
  );
  await searchInSearchbar("searchbar");
  info("Click the first one-off button.");
  BrowserSearch.searchBar.textbox.popup.oneOffButtons
    .getSelectableButtons(false)[0]
    .click();
  await promiseLoad;

  await BrowserTestUtils.waitForCondition(
    () => gRequests.length == 1,
    "Should have received an attribution submission"
  );
  Assert.equal(
    gRequests[0].getHeader("X-Region"),
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
  // Set back the engine in case of other tests in this file.
  await Services.search.setDefault(Services.search.getEngineByName("basic"));
  gRequests = [];
});
