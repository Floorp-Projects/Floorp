/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Browser test for the weather suggestion.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

add_setup(async function() {
  await MerinoTestUtils.initWeather();
});

// This test ensures the browser navigates to the weather webpage after
// the weather result is selected.
add_task(async function test_weather_result_selection() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: MerinoTestUtils.WEATHER_KEYWORD,
  });

  info(`Select the weather result`);
  EventUtils.synthesizeKey("KEY_ArrowDown");

  info(`Navigate to the weather url`);
  EventUtils.synthesizeKey("KEY_Enter");

  await browserLoadedPromise;

  Assert.equal(
    gBrowser.currentURI.spec,
    "https://example.com/weather",
    "Assert the page navigated to the weather webpage after selecting the weather result."
  );

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.history.clear();
});

// Tests the "Show less frequently" result menu command.
add_task(async function showLessFrequently() {
  // Set up a min keyword length and cap.
  QuickSuggest.weather._test_setRsData({
    keywords: ["weather"],
    min_keyword_length: 3,
    min_keyword_length_cap: 4,
  });

  // Trigger the suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "wea",
  });

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.result.providerName,
    UrlbarProviderWeather.name,
    "Weather suggestion should be present at expected index after 'wea' search"
  );

  // Click the command.
  let command = "show_less_frequently";
  await UrlbarTestUtils.openResultMenuAndClickItem(window, command, {
    resultIndex,
  });

  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open clicking the command"
  );
  Assert.ok(
    details.element.row.hasAttribute("feedback-acknowledgment"),
    "Row should have feedback acknowledgment after clicking command"
  );

  // Do the same search again. The suggestion should not appear.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "wea",
  });

  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.notEqual(
      details.result.providerName,
      UrlbarProviderWeather.name,
      `Weather suggestion should be absent (checking index ${i})`
    );
  }

  // Do a search using one more character. The suggestion should appear.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "weat",
  });

  details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.result.providerName,
    UrlbarProviderWeather.name,
    "Weather suggestion should be present at expected index after 'weat' search"
  );
  Assert.ok(
    !details.element.row.hasAttribute("feedback-acknowledgment"),
    "Row should not have feedback acknowledgment after 'weat' search"
  );

  // Since the cap has been reached, the command should no longer appear in the
  // result menu.
  await UrlbarTestUtils.openResultMenu(window, { resultIndex });
  let menuitem = gURLBar.view.resultMenu.querySelector(
    `menuitem[data-command=${command}]`
  );
  Assert.ok(!menuitem, "Menuitem should be absent");
  gURLBar.view.resultMenu.hidePopup(true);

  await UrlbarTestUtils.promisePopupClose(window);
  QuickSuggest.weather._test_setRsData(MerinoTestUtils.WEATHER_RS_DATA);
  UrlbarPrefs.clear("weather.minKeywordLength");
});

// Tests the "Not interested" result menu dismissal command.
add_task(async function notInterested() {
  await doDismissTest("not_interested");
});

// Tests the "Not relevant" result menu dismissal command.
add_task(async function notRelevant() {
  await doDismissTest("not_relevant");
});

async function doDismissTest(command) {
  // Trigger the suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: MerinoTestUtils.WEATHER_KEYWORD,
  });

  let resultCount = UrlbarTestUtils.getResultCount(window);

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.result.providerName,
    UrlbarProviderWeather.name,
    "Weather suggestion should be present"
  );

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(
    window,
    ["[data-l10n-id=firefox-suggest-weather-command-dont-show-this]", command],
    { resultIndex }
  );

  Assert.ok(
    !UrlbarPrefs.get("suggest.weather"),
    "suggest.weather pref should be set to false after dismissal"
  );

  // The row should be a tip now.
  Assert.ok(gURLBar.view.isOpen, "The view should remain open after dismissal");
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount,
    "The result count should not haved changed after dismissal"
  );
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  Assert.equal(
    details.type,
    UrlbarUtils.RESULT_TYPE.TIP,
    "Row should be a tip after dismissal"
  );
  Assert.equal(
    details.result.payload.type,
    "dismissalAcknowledgment",
    "Tip type should be dismissalAcknowledgment"
  );

  // Get the dismissal acknowledgment's "Got it" button and click it.
  let gotItButton = UrlbarTestUtils.getButtonForResultIndex(
    window,
    "0",
    resultIndex
  );
  Assert.ok(gotItButton, "Row should have a 'Got it' button");
  EventUtils.synthesizeMouseAtCenter(gotItButton, {}, window);

  // The view should remain open and the tip row should be gone.
  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open clicking the 'Got it' button"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    resultCount - 1,
    "The result count should be one less after clicking 'Got it' button"
  );
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      details.type != UrlbarUtils.RESULT_TYPE.TIP &&
        details.result.providerName != UrlbarProviderWeather.name,
      "Tip result and weather result should not be present"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);

  // Enable the weather suggestion again and wait for it to be fetched.
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  UrlbarPrefs.clear("suggest.weather");
  info("Waiting for weather fetch after re-enabling the suggestion");
  await fetchPromise;
  info("Got weather fetch");
}
