/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

let gMaxResults;

add_task(async function init() {
  Services.prefs.setBoolPref("browser.urlbar.oneOffSearches", true);
  gMaxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");

  // Add a search suggestion engine and move it to the front so that it appears
  // as the first one-off.
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  Services.search.moveEngine(engine, 0);

  registerCleanupFunction(async function() {
    await hidePopup();
    await PlacesUtils.history.clear();
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


async function selectSettings(activateFn) {
  await BrowserTestUtils.withNewTab({gBrowser, url: "about:blank"}, async browser => {
    gURLBar.focus();
    EventUtils.synthesizeKey("KEY_ArrowDown");
    await promisePopupShown(gURLBar.popup);
    await waitForAutocompleteResultAt(gMaxResults - 1);

    let promiseHidden = promisePopupHidden(gURLBar.popup);
    let prefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded", () => true);

    activateFn();

    await prefPaneLoaded;
    await promiseHidden;

    Assert.equal(gBrowser.contentWindow.history.state, "paneSearch",
      "Should have opened the search preferences pane");
  });
}

add_task(async function test_open_settings_with_enter() {
  await selectSettings(() => {
    EventUtils.synthesizeKey("KEY_ArrowUp");

    Assert.ok(gURLBar.popup.oneOffSearchButtons.selectedButton
      .classList.contains("search-setting-button-compact"),
      "Should have selected the settings button");

    EventUtils.synthesizeKey("KEY_Enter");
  });
});

add_task(async function test_open_settings_with_click() {
  await selectSettings(() => {
    gURLBar.popup.oneOffSearchButtons.settingsButton.click();
  });
});

async function hidePopup() {
  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
}
