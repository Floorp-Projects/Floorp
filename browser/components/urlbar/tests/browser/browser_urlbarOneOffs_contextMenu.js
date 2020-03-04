/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the right-click menu works correctly for the one-off buttons.
 */

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

let gMaxResults;

XPCOMUtils.defineLazyGetter(this, "oneOffSearchButtons", () => {
  return UrlbarTestUtils.getOneOffSearchButtons(window);
});

let originalEngine;
let newEngine;

add_task(async function setup() {
  SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.oneOffSearches", true],
      // Avoid hitting the network with search suggestions.
      ["browser.urlbar.suggest.searches", false],
      ["browser.tabs.loadInBackground", true],
    ],
  });
  gMaxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");

  // Add a search suggestion engine and move it to the front so that it appears
  // as the first one-off.
  originalEngine = await Services.search.getDefault();
  newEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  await Services.search.moveEngine(newEngine, 0);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await Services.search.setDefault(originalEngine);
  });

  await PlacesUtils.history.clear();

  let visits = [];
  for (let i = 0; i < gMaxResults; i++) {
    visits.push({
      uri: makeURI("http://example.com/browser_urlbarOneOffs.js/?" + i),
      // TYPED so that the visit shows up when the urlbar's drop-down arrow is
      // pressed.
      transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
    });
  }
  await PlacesTestUtils.addVisits(visits);
});

async function searchInTab(checkFn) {
  // Ensure we've got a different engine selected to the one we added. so that
  // it is a different engine to select.
  await Services.search.setDefault(originalEngine);

  await BrowserTestUtils.withNewTab({ gBrowser }, async testBrowser => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus: SimpleTest.waitForFocus,
      value: "foo",
    });

    let contextMenu = oneOffSearchButtons.querySelector(
      ".search-one-offs-context-menu"
    );
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );
    let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
    EventUtils.synthesizeMouseAtCenter(oneOffs[0], {
      type: "contextmenu",
      button: 2,
    });
    await popupShownPromise;

    let tabOpenAndLoaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

    let openInTab = oneOffSearchButtons.querySelector(
      ".search-one-offs-context-open-in-new-tab"
    );
    EventUtils.synthesizeMouseAtCenter(openInTab, {});

    let newTab = await tabOpenAndLoaded;

    checkFn(testBrowser, newTab);

    BrowserTestUtils.removeTab(newTab);
  });
}

add_task(async function searchInNewTab_opensBackground() {
  Services.prefs.setBoolPref("browser.tabs.loadInBackground", true);
  await searchInTab((testBrowser, newTab) => {
    Assert.equal(
      newTab.linkedBrowser.currentURI.spec,
      "http://mochi.test:8888/?terms=foo",
      "Should have loaded the expected URI in a new tab."
    );

    Assert.equal(
      testBrowser.currentURI.spec,
      "about:blank",
      "Should not have touched the original tab"
    );

    Assert.equal(
      testBrowser,
      gBrowser.selectedTab.linkedBrowser,
      "Should not have changed the selected tab"
    );
  });
});

add_task(async function searchInNewTab_opensForeground() {
  Services.prefs.setBoolPref("browser.tabs.loadInBackground", false);

  await searchInTab((testBrowser, newTab) => {
    Assert.equal(
      newTab.linkedBrowser.currentURI.spec,
      "http://mochi.test:8888/?terms=foo",
      "Should have loaded the expected URI in a new tab."
    );

    Assert.equal(
      testBrowser.currentURI.spec,
      "about:blank",
      "Should not have touched the original tab"
    );

    Assert.equal(
      newTab,
      gBrowser.selectedTab,
      "Should have changed the selected tab"
    );
  });
});

add_task(async function switchDefaultEngine() {
  await Services.search.setDefault(originalEngine);

  await BrowserTestUtils.withNewTab({ gBrowser }, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      waitForFocus: SimpleTest.waitForFocus,
      value: "foo",
    });

    let contextMenu = oneOffSearchButtons.querySelector(
      ".search-one-offs-context-menu"
    );
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );
    let oneOffs = oneOffSearchButtons.getSelectableButtons(true);
    EventUtils.synthesizeMouseAtCenter(oneOffs[0], {
      type: "contextmenu",
      button: 2,
    });
    await popupShownPromise;

    let engineChangedPromise = SearchTestUtils.promiseSearchNotification(
      "engine-default",
      "browser-search-engine-modified"
    );
    let setDefault = oneOffSearchButtons.querySelector(
      ".search-one-offs-context-set-default"
    );
    EventUtils.synthesizeMouseAtCenter(setDefault, {});
    await engineChangedPromise;

    Assert.equal(
      await Services.search.getDefault(),
      newEngine,
      "Should have correctly changed the engine to the new one"
    );
  });
});
