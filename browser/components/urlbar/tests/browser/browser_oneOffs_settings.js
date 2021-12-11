/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests that the settings button in the one-off buttons display correctly
 * loads the search preferences.
 */

let gMaxResults;

add_task(async function init() {
  gMaxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });

  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();

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

async function selectSettings(win, activateFn) {
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "about:blank" },
    async browser => {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window: win,
        value: "example.com",
      });
      await UrlbarTestUtils.waitForAutocompleteResultAt(win, gMaxResults - 1);

      await UrlbarTestUtils.promisePopupClose(win, async () => {
        let prefPaneLoaded = TestUtils.topicObserved(
          "sync-pane-loaded",
          () => true
        );

        activateFn();

        await prefPaneLoaded;
      });

      Assert.equal(
        win.gBrowser.contentWindow.history.state,
        "paneSearch",
        "Should have opened the search preferences pane"
      );
    }
  );
}

add_task(async function test_open_settings_with_enter() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await selectSettings(win, () => {
    EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);

    Assert.ok(
      UrlbarTestUtils.getOneOffSearchButtons(
        win
      ).selectedButton.classList.contains("search-setting-button"),
      "Should have selected the settings button"
    );

    EventUtils.synthesizeKey("KEY_Enter", {}, win);
  });
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_open_settings_with_click() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await selectSettings(win, () => {
    UrlbarTestUtils.getOneOffSearchButtons(win).settingsButton.click();
  });
  await BrowserTestUtils.closeWindow(win);
});
