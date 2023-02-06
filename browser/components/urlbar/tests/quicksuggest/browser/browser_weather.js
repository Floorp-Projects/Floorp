/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures the browser navigates to the weather webpage after
 * the weather result is selected.
 */

"use strict";

add_setup(async function() {
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_weather_result_selection() {
  await MerinoTestUtils.initWeather();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
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
});
