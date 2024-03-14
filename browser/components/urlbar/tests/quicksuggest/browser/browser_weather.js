/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Browser test for the weather suggestion.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

// This test takes a while and can time out in verify mode. Each task is run
// twice, once with Rust enabled and once with it disabled. Once we remove the
// JS backend this should improve a lot, but for now request a longer timeout.
requestLongerTimeout(5);

add_setup(async function () {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "weather",
        weather: MerinoTestUtils.WEATHER_RS_DATA,
      },
    ],
  });
  await MerinoTestUtils.initWeather();

  // When `add_tasks_with_rust()` disables the Rust backend and forces sync, the
  // JS backend will sync `Weather` with remote settings. Since keywords are
  // present in remote settings at that point (we added them above), `Weather`
  // will then start fetching. The fetch may or may not be done before our test
  // task starts. To make sure it's done, queue another fetch and await it.
  registerAddTasksWithRustSetup(async () => {
    await QuickSuggest.weather._test_fetch();
  });
});

// Basic checks of the row DOM.
add_tasks_with_rust(async function dom() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: MerinoTestUtils.WEATHER_KEYWORD,
  });

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  assertIsWeatherResult(details.result, true);
  let { row } = details.element;

  Assert.ok(
    BrowserTestUtils.isVisible(
      row.querySelector(".urlbarView-title-separator")
    ),
    "The title separator should be visible"
  );

  await UrlbarTestUtils.promisePopupClose(window);
});

// This test ensures the browser navigates to the weather webpage after
// the weather result is selected.
add_tasks_with_rust(async function test_weather_result_selection() {
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

// Does a search, clicks the "Show less frequently" result menu command, and
// repeats both steps until the min keyword length cap is reached.
add_tasks_with_rust(async function showLessFrequentlyCapReached_manySearches() {
  // Set up a min keyword length and cap.
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: {
        keywords: ["weather"],
        min_keyword_length: 3,
      },
    },
    {
      type: "configuration",
      configuration: {
        show_less_frequently_cap: 1,
      },
    },
  ]);

  // Trigger the suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "wea",
  });

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  info("Weather suggestion should be present after 'wea' search");
  assertIsWeatherResult(details.result, true);

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
  Assert.equal(
    UrlbarPrefs.get("weather.minKeywordLength"),
    4,
    "weather.minKeywordLength should be incremented once"
  );

  // Do the same search again. The suggestion should not appear.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "wea",
  });

  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    info(`Weather suggestion should be absent (checking index ${i})`);
    assertIsWeatherResult(details.result, false);
  }

  // Do a search using one more character. The suggestion should appear.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "weat",
  });

  details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  info("Weather suggestion should be present after 'weat' search");
  assertIsWeatherResult(details.result, true);
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
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: MerinoTestUtils.WEATHER_RS_DATA,
    },
  ]);
  UrlbarPrefs.clear("weather.minKeywordLength");
});

// Repeatedly clicks the "Show less frequently" result menu command after doing
// a single search until the min keyword length cap is reached.
add_tasks_with_rust(async function showLessFrequentlyCapReached_oneSearch() {
  // Set up a min keyword length and cap.
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: {
        keywords: ["weather"],
        min_keyword_length: 3,
      },
    },
    {
      type: "configuration",
      configuration: {
        show_less_frequently_cap: 3,
      },
    },
  ]);

  // Trigger the suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "wea",
  });

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  info("Weather suggestion should be present after 'wea' search");
  assertIsWeatherResult(details.result, true);

  let command = "show_less_frequently";

  for (let i = 0; i < 3; i++) {
    await UrlbarTestUtils.openResultMenuAndClickItem(window, command, {
      resultIndex,
      openByMouse: true,
    });

    Assert.ok(
      gURLBar.view.isOpen,
      "The view should remain open clicking the command"
    );
    Assert.ok(
      details.element.row.hasAttribute("feedback-acknowledgment"),
      "Row should have feedback acknowledgment after clicking command"
    );
    Assert.equal(
      UrlbarPrefs.get("weather.minKeywordLength"),
      4 + i,
      "weather.minKeywordLength should be incremented once"
    );
  }

  let menuitem = await UrlbarTestUtils.openResultMenuAndGetItem({
    window,
    command,
    resultIndex,
  });
  Assert.ok(
    !menuitem,
    "The menuitem should not exist after the cap is reached"
  );

  gURLBar.view.resultMenu.hidePopup(true);
  await UrlbarTestUtils.promisePopupClose(window);
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: MerinoTestUtils.WEATHER_RS_DATA,
    },
  ]);
  UrlbarPrefs.clear("weather.minKeywordLength");
});

// Tests the "Not interested" result menu dismissal command.
add_tasks_with_rust(async function notInterested() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: MerinoTestUtils.WEATHER_KEYWORD,
  });
  await doDismissTest("not_interested");
});

// Tests the "Not relevant" result menu dismissal command.
add_tasks_with_rust(async function notRelevant() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: MerinoTestUtils.WEATHER_KEYWORD,
  });
  await doDismissTest("not_relevant");
});

async function doDismissTest(command) {
  let resultCount = UrlbarTestUtils.getResultCount(window);

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  assertIsWeatherResult(details.result, true);

  // Click the command.
  await UrlbarTestUtils.openResultMenuAndClickItem(
    window,
    ["[data-l10n-id=firefox-suggest-command-dont-show-this]", command],
    { resultIndex, openByMouse: true }
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
  Assert.ok(
    !details.element.row.hasAttribute("feedback-acknowledgment"),
    "Row should not have feedback acknowledgment after dismissal"
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
    Assert.notEqual(
      details.type,
      UrlbarUtils.RESULT_TYPE.TIP,
      "Tip result should not be present"
    );
    info("Weather result should not be present");
    assertIsWeatherResult(details.result, false);
  }

  await UrlbarTestUtils.promisePopupClose(window);

  // Enable the weather suggestion again and wait for it to be fetched.
  UrlbarPrefs.clear("suggest.weather");
  await QuickSuggest.weather._test_fetch();

  // Wait for keywords to be re-synced from remote settings.
  await QuickSuggestTestUtils.forceSync();
}

// Tests the "Report inaccurate location" result menu command immediately
// followed by a dismissal command to make sure other commands still work
// properly while the urlbar session remains ongoing.
add_tasks_with_rust(async function inaccurateLocationAndDismissal() {
  await doSessionOngoingCommandTest("inaccurate_location");
});

// Tests the "Show less frequently" result menu command immediately followed by
// a dismissal command to make sure other commands still work properly while the
// urlbar session remains ongoing.
add_tasks_with_rust(async function showLessFrequentlyAndDismissal() {
  await doSessionOngoingCommandTest("show_less_frequently");
  UrlbarPrefs.clear("weather.minKeywordLength");
});

async function doSessionOngoingCommandTest(command) {
  // Trigger the suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: MerinoTestUtils.WEATHER_KEYWORD,
  });

  let resultIndex = 1;
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, resultIndex);
  info("Weather suggestion should be present after search");
  assertIsWeatherResult(details.result, true);

  // Click the command.
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

  info("Doing dismissal");
  await doDismissTest("not_interested");
}

// Test for menu item to mange the suggest.
add_tasks_with_rust(async function manage() {
  await BrowserTestUtils.withNewTab({ gBrowser }, async browser => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: MerinoTestUtils.WEATHER_KEYWORD,
    });

    let resultIndex = 1;
    let details = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      resultIndex
    );
    assertIsWeatherResult(details.result, true);

    const managePage = "about:preferences#search";
    let onManagePageLoaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      managePage
    );
    // Click the command.
    await UrlbarTestUtils.openResultMenuAndClickItem(window, "manage", {
      resultIndex,
    });
    await onManagePageLoaded;
    Assert.equal(
      browser.currentURI.spec,
      managePage,
      "The manage page is loaded"
    );

    await UrlbarTestUtils.promisePopupClose(window);
  });
});

// Test for simple UI.
add_tasks_with_rust(async function simpleUI() {
  const testData = [
    {
      weatherSimpleUI: true,
      expectedSummary:
        MerinoTestUtils.WEATHER_SUGGESTION.current_conditions.summary,
    },
    {
      weatherSimpleUI: false,
      expectedSummary: `${MerinoTestUtils.WEATHER_SUGGESTION.current_conditions.summary}; ${MerinoTestUtils.WEATHER_SUGGESTION.forecast.summary}`,
    },
  ];

  for (let { weatherSimpleUI, expectedSummary } of testData) {
    let nimbusCleanup = await UrlbarTestUtils.initNimbusFeature({
      weatherSimpleUI,
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: MerinoTestUtils.WEATHER_KEYWORD,
    });

    let resultIndex = 1;
    let details = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      resultIndex
    );
    assertIsWeatherResult(details.result, true);

    let { row } = details.element;
    let summary = row.querySelector(".urlbarView-dynamic-weather-summaryText");

    // `getViewUpdate()` is allowed to be async and `UrlbarView` awaits it even
    // though the `Weather` implementation is not async. That means the summary
    // text content will be updated asyncly, so we need to wait for it.
    await TestUtils.waitForCondition(
      () => summary.textContent == expectedSummary,
      "Waiting for the row's summary text to be updated"
    );
    Assert.equal(
      summary.textContent,
      expectedSummary,
      "The summary text should be correct"
    );

    await UrlbarTestUtils.promisePopupClose(window);
    await nimbusCleanup();
  }
});

function assertIsWeatherResult(result, isWeatherResult) {
  let provider = UrlbarPrefs.get("quickSuggestRustEnabled")
    ? UrlbarProviderQuickSuggest
    : UrlbarProviderWeather;
  if (isWeatherResult) {
    Assert.equal(
      result.providerName,
      provider.name,
      "Result should be from a weather provider"
    );
    Assert.equal(
      UrlbarUtils.searchEngagementTelemetryType(result),
      "weather",
      "Result telemetry type should be 'weather'"
    );
  } else {
    Assert.notEqual(
      result.providerName,
      provider.name,
      "Result should not be from a weather provider"
    );
    Assert.notEqual(
      UrlbarUtils.searchEngagementTelemetryType(result),
      "weather",
      "Result telemetry type should not be 'weather'"
    );
  }
}
