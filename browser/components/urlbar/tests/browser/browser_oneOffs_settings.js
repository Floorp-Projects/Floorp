/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests that the settings button in the one-off buttons display correctly
 * loads the search preferences.
 */

let gMaxResults;

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.oneOffSearches", true]],
  });
  gMaxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");

  registerCleanupFunction(async function() {
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
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        waitForFocus: SimpleTest.waitForFocus,
        value: "example.com",
      });
      await UrlbarTestUtils.waitForAutocompleteResultAt(
        window,
        gMaxResults - 1
      );

      await UrlbarTestUtils.promisePopupClose(window, async () => {
        let prefPaneLoaded = TestUtils.topicObserved(
          "sync-pane-loaded",
          () => true
        );

        activateFn();

        await prefPaneLoaded;
      });

      Assert.equal(
        gBrowser.contentWindow.history.state,
        "paneSearch",
        "Should have opened the search preferences pane"
      );
    }
  );
}

add_task(async function test_open_settings_with_enter() {
  await selectSettings(() => {
    EventUtils.synthesizeKey("KEY_ArrowUp");

    Assert.ok(
      UrlbarTestUtils.getOneOffSearchButtons(
        window
      ).selectedButton.classList.contains("search-setting-button-compact"),
      "Should have selected the settings button"
    );

    EventUtils.synthesizeKey("KEY_Enter");
  });
});

add_task(async function test_open_settings_with_click() {
  await selectSettings(() => {
    UrlbarTestUtils.getOneOffSearchButtons(window).settingsButton.click();
  });
});
