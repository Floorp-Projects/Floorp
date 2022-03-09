/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests urlbar telemetry for Quick Suggest results.
 * See also browser_quicksuggest_onboardingDialog.js for onboarding telemetry.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

const TEST_URL = "http://example.com/quicksuggest?q=frabbits";
const TEST_SEARCH_STRING = "frab";

const BEST_MATCH_URL = "http://example.com/best-match";
const BEST_MATCH_SEARCH_STRING = "bestmatch";

const TEST_DATA = [
  {
    id: 1,
    url: TEST_URL,
    title: "frabbits",
    keywords: [TEST_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "Test-Advertiser",
  },
  {
    id: 2,
    url: BEST_MATCH_URL,
    title: "Best match",
    keywords: [BEST_MATCH_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "Test-Advertiser",
    _test_is_best_match: true,
  },
];

const EXPERIMENT_PREF = "browser.urlbar.quicksuggest.enabled";
const SUGGEST_PREF = "suggest.quicksuggest.nonsponsored";

// Spy for the custom impression/click sender
let spy;

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_task(async function init() {
  ({ sandbox, spy } = QuickSuggestTestUtils.createTelemetryPingSpy());

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension();
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(Services.search.getEngineByName("Example"));

  // Set up Quick Suggest.
  await QuickSuggestTestUtils.ensureQuickSuggestInit(TEST_DATA);

  // Enable local telemetry recording for the duration of the test.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  Services.telemetry.clearScalars();

  registerCleanupFunction(async () => {
    Services.search.setDefault(oldDefaultEngine);
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

// Tests the following:
// * impression telemetry
// * offline scenario
// * data collection disabled
add_task(async function impression_offline_dataCollectionDisabled() {
  await QuickSuggestTestUtils.setScenario("offline");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  await doImpressionTest({
    scenario: "offline",
  });
});

// Tests the following:
// * impression telemetry
// * offline scenario
// * data collection enabled
add_task(async function impression_offline_dataCollectionEnabled() {
  await QuickSuggestTestUtils.setScenario("offline");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  await doImpressionTest({
    scenario: "offline",
  });
});

// Tests the following:
// * impression telemetry
// * online scenario
// * data collection disabled
add_task(async function impression_online_dataCollectionDisabled() {
  await QuickSuggestTestUtils.setScenario("online");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await doImpressionTest({
    scenario: "online",
  });
});

// Tests the following:
// * impression telemetry
// * online scenario
// * data collection enabled
add_task(async function impression_online_dataCollectionEnabled() {
  await QuickSuggestTestUtils.setScenario("online");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await doImpressionTest({
    scenario: "online",
  });
});

async function doImpressionTest({ scenario }) {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    let index = 1;
    await QuickSuggestTestUtils.assertIsQuickSuggest({
      window,
      index,
      url: TEST_URL,
    });
    // Press Enter on the heuristic result, which is not the quick suggest, to
    // make sure we don't record click telemetry.
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
    QuickSuggestTestUtils.assertScalars({
      [QuickSuggestTestUtils.SCALARS.IMPRESSION]: index + 1,
    });
    QuickSuggestTestUtils.assertImpressionPing({
      index,
      spy,
      scenario,
    });
    QuickSuggestTestUtils.assertNoClickPing(spy);
  });

  await PlacesUtils.history.clear();

  await QuickSuggestTestUtils.setScenario(null);
  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
}

// Makes sure impression telemetry is not recorded when the urlbar engagement is
// abandoned.
add_task(async function noImpression_abandonment() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    await QuickSuggestTestUtils.assertIsQuickSuggest({
      window,
      url: TEST_URL,
    });
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
    QuickSuggestTestUtils.assertScalars({});
    QuickSuggestTestUtils.assertNoImpressionPing(spy);
    QuickSuggestTestUtils.assertNoClickPing(spy);
  });
});

// Makes sure impression telemetry is not recorded when a quick suggest result
// is not present.
add_task(async function noImpression_noQuickSuggestResult() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "noImpression_noQuickSuggestResult",
      fireInputEvent: true,
    });
    await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
    QuickSuggestTestUtils.assertScalars({});
    QuickSuggestTestUtils.assertNoImpressionPing(spy);
    QuickSuggestTestUtils.assertNoClickPing(spy);
  });
  await PlacesUtils.history.clear();
});

// Tests the following:
// * click telemetry using keyboard
// * offline scenario
// * data collection disabled
add_task(async function click_keyboard_offline_dataCollectionDisabled() {
  await QuickSuggestTestUtils.setScenario("offline");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  await doClickTest({
    useKeyboard: true,
    scenario: "offline",
  });
});

// Tests the following:
// * click telemetry using keyboard
// * offline scenario
// * data collection enabled
add_task(async function click_keyboard_offline_dataCollectionEnabled() {
  await QuickSuggestTestUtils.setScenario("offline");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  await doClickTest({
    useKeyboard: true,
    scenario: "offline",
  });
});

// Tests the following:
// * click telemetry using keyboard
// * online scenario
// * data collection disabled
add_task(async function click_keyboard_online_dataCollectionDisabled() {
  await QuickSuggestTestUtils.setScenario("online");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await doClickTest({
    useKeyboard: true,
    scenario: "online",
  });
});

// Tests the following:
// * click telemetry using keyboard
// * online scenario
// * data collection enabled
add_task(async function click_keyboard_online_dataCollectionEnabled() {
  await QuickSuggestTestUtils.setScenario("online");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await doClickTest({
    useKeyboard: true,
    scenario: "online",
  });
});

// Tests the following:
// * click telemetry using mouse
// * offline scenario
// * data collection disabled
add_task(async function click_mouse_offline_dataCollectionDisabled() {
  await QuickSuggestTestUtils.setScenario("offline");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  await doClickTest({
    useKeyboard: false,
    scenario: "offline",
  });
});

// Tests the following:
// * click telemetry using mouse
// * offline scenario
// * data collection enabled
add_task(async function click_mouse_offline_dataCollectionEnabled() {
  await QuickSuggestTestUtils.setScenario("offline");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  await doClickTest({
    useKeyboard: false,
    scenario: "offline",
  });
});

// Tests the following:
// * click telemetry using mouse
// * online scenario
// * data collection disabled
add_task(async function click_mouse_online_dataCollectionDisabled() {
  await QuickSuggestTestUtils.setScenario("online");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await doClickTest({
    useKeyboard: false,
    scenario: "online",
  });
});

// Tests the following:
// * click telemetry using mouse
// * online scenario
// * data collection enabled
add_task(async function click_mouse_online_dataCollectionEnabled() {
  await QuickSuggestTestUtils.setScenario("online");
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await doClickTest({
    useKeyboard: false,
    scenario: "online",
  });
});

// Tests impression and click telemetry for best matches.
add_task(async function bestMatch() {
  UrlbarPrefs.set("bestMatch.enabled", true);
  await QuickSuggestTestUtils.setScenario("offline");
  await doClickTest({
    useKeyboard: false,
    scenario: "offline",
    searchString: BEST_MATCH_SEARCH_STRING,
    url: BEST_MATCH_URL,
    block_id: 2,
    isBestMatch: true,
  });
  UrlbarPrefs.clear("bestMatch.enabled");
});

async function doClickTest({
  useKeyboard,
  scenario,
  block_id = undefined,
  isBestMatch = false,
  searchString = TEST_SEARCH_STRING,
  url = TEST_URL,
}) {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });
    let index = 1;
    let result = await QuickSuggestTestUtils.assertIsQuickSuggest({
      window,
      index,
      url,
      isBestMatch,
    });
    await UrlbarTestUtils.promisePopupClose(window, () => {
      if (useKeyboard) {
        EventUtils.synthesizeKey("KEY_ArrowDown");
        EventUtils.synthesizeKey("KEY_Enter");
      } else {
        EventUtils.synthesizeMouseAtCenter(result.element.row, {});
      }
    });
    QuickSuggestTestUtils.assertScalars({
      [QuickSuggestTestUtils.SCALARS.IMPRESSION]: index + 1,
      [QuickSuggestTestUtils.SCALARS.CLICK]: index + 1,
    });
    let match_type = isBestMatch ? "best-match" : "firefox-suggest";
    QuickSuggestTestUtils.assertImpressionPing({
      index,
      spy,
      scenario,
      block_id,
      match_type,
      is_clicked: true,
    });
    QuickSuggestTestUtils.assertClickPing({
      index,
      spy,
      scenario,
      block_id,
      match_type,
    });
  });

  await PlacesUtils.history.clear();

  await QuickSuggestTestUtils.setScenario(null);
  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
}

// Tests the impression and click scalars and the custom click ping by picking a
// Quick Suggest result when it's shown before search suggestions.
add_task(async function click_beforeSearchSuggestions() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchSuggestionsFirst", false]],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await withSuggestions(async () => {
      spy.resetHistory();
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: TEST_SEARCH_STRING,
        fireInputEvent: true,
      });
      let resultCount = UrlbarTestUtils.getResultCount(window);
      Assert.greaterOrEqual(
        resultCount,
        4,
        "Result count >= 1 heuristic + 1 quick suggest + 2 suggestions"
      );
      let index = resultCount - 3;
      await QuickSuggestTestUtils.assertIsQuickSuggest({
        window,
        index,
        url: TEST_URL,
      });
      await UrlbarTestUtils.promisePopupClose(window, () => {
        EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: index });
        EventUtils.synthesizeKey("KEY_Enter");
      });
      // Arrow down to the quick suggest result and press Enter.
      QuickSuggestTestUtils.assertScalars({
        [QuickSuggestTestUtils.SCALARS.IMPRESSION]: index + 1,
        [QuickSuggestTestUtils.SCALARS.CLICK]: index + 1,
      });
      QuickSuggestTestUtils.assertImpressionPing({
        index,
        spy,
        is_clicked: true,
      });
      QuickSuggestTestUtils.assertClickPing({ index, spy });
    });
  });
  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
});

// Tests the help scalar by picking a Quick Suggest result help button with the
// keyboard.
add_task(async function help_keyboard() {
  spy.resetHistory();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_SEARCH_STRING,
    fireInputEvent: true,
  });
  let index = 1;
  let result = await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index,
    url: TEST_URL,
  });
  let helpButton = result.element.row._buttons.get("help");
  Assert.ok(helpButton, "The result has a help button");
  let helpLoadPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
    EventUtils.synthesizeKey("KEY_Enter");
  });
  await helpLoadPromise;
  Assert.equal(
    gBrowser.currentURI.spec,
    QuickSuggestTestUtils.LEARN_MORE_URL,
    "Help URL loaded"
  );
  QuickSuggestTestUtils.assertScalars({
    [QuickSuggestTestUtils.SCALARS.IMPRESSION]: index + 1,
    [QuickSuggestTestUtils.SCALARS.HELP]: index + 1,
  });
  QuickSuggestTestUtils.assertImpressionPing({ index, spy });
  QuickSuggestTestUtils.assertNoClickPing(spy);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await PlacesUtils.history.clear();
});

// Tests the help scalar by picking a Quick Suggest result help button with the
// mouse.
add_task(async function help_mouse() {
  spy.resetHistory();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_SEARCH_STRING,
    fireInputEvent: true,
  });
  let index = 1;
  let result = await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index,
    url: TEST_URL,
  });
  let helpButton = result.element.row._buttons.get("help");
  Assert.ok(helpButton, "The result has a help button");
  let helpLoadPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(helpButton, {});
  });
  await helpLoadPromise;
  Assert.equal(
    gBrowser.currentURI.spec,
    QuickSuggestTestUtils.LEARN_MORE_URL,
    "Help URL loaded"
  );
  QuickSuggestTestUtils.assertScalars({
    [QuickSuggestTestUtils.SCALARS.IMPRESSION]: index + 1,
    [QuickSuggestTestUtils.SCALARS.HELP]: index + 1,
  });
  QuickSuggestTestUtils.assertImpressionPing({ index, spy });
  QuickSuggestTestUtils.assertNoClickPing(spy);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await PlacesUtils.history.clear();
});

// Tests telemetry recorded when toggling the
// `suggest.quicksuggest.nonsponsored` pref:
// * contextservices.quicksuggest enable_toggled event telemetry
// * TelemetryEnvironment
add_task(async function enableToggled() {
  Services.telemetry.clearEvents();

  // Toggle the suggest.quicksuggest.nonsponsored pref twice. We should get two
  // events.
  let enabled = UrlbarPrefs.get(SUGGEST_PREF);
  for (let i = 0; i < 2; i++) {
    enabled = !enabled;
    UrlbarPrefs.set(SUGGEST_PREF, enabled);
    TelemetryTestUtils.assertEvents([
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "enable_toggled",
        object: enabled ? "enabled" : "disabled",
      },
    ]);
    Assert.equal(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        "browser.urlbar.suggest.quicksuggest.nonsponsored"
      ],
      enabled,
      "suggest.quicksuggest.nonsponsored is correct in TelemetryEnvironment"
    );
  }

  // Set the main quicksuggest.enabled pref to false and toggle the
  // suggest.quicksuggest.nonsponsored pref again.  We shouldn't get any events.
  await SpecialPowers.pushPrefEnv({
    set: [[EXPERIMENT_PREF, false]],
  });
  enabled = !enabled;
  UrlbarPrefs.set(SUGGEST_PREF, enabled);
  TelemetryTestUtils.assertEvents([], {
    category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
  });
  await SpecialPowers.popPrefEnv();

  // Set the pref back to what it was at the start of the task.
  UrlbarPrefs.set(SUGGEST_PREF, !enabled);
});

// Tests telemetry recorded when toggling the `suggest.quicksuggest.sponsored`
// pref:
// * contextservices.quicksuggest enable_toggled event telemetry
// * TelemetryEnvironment
add_task(async function sponsoredToggled() {
  Services.telemetry.clearEvents();

  // Toggle the suggest.quicksuggest.sponsored pref twice. We should get two
  // events.
  let enabled = UrlbarPrefs.get("suggest.quicksuggest.sponsored");
  for (let i = 0; i < 2; i++) {
    enabled = !enabled;
    UrlbarPrefs.set("suggest.quicksuggest.sponsored", enabled);
    TelemetryTestUtils.assertEvents([
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "sponsored_toggled",
        object: enabled ? "enabled" : "disabled",
      },
    ]);
    Assert.equal(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        "browser.urlbar.suggest.quicksuggest.sponsored"
      ],
      enabled,
      "suggest.quicksuggest.sponsored is correct in TelemetryEnvironment"
    );
  }

  // Set the main quicksuggest.enabled pref to false and toggle the
  // suggest.quicksuggest.sponsored pref again. We shouldn't get any events.
  await SpecialPowers.pushPrefEnv({
    set: [[EXPERIMENT_PREF, false]],
  });
  enabled = !enabled;
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", enabled);
  TelemetryTestUtils.assertEvents([], {
    category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
  });
  await SpecialPowers.popPrefEnv();

  // Set the pref back to what it was at the start of the task.
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", !enabled);
});

// Tests telemetry recorded when toggling the
// `quicksuggest.dataCollection.enabled` pref:
// * contextservices.quicksuggest data_collect_toggled event telemetry
// * TelemetryEnvironment
add_task(async function dataCollectionToggled() {
  Services.telemetry.clearEvents();

  // Toggle the quicksuggest.dataCollection.enabled pref twice. We should get
  // two events.
  let enabled = UrlbarPrefs.get("quicksuggest.dataCollection.enabled");
  for (let i = 0; i < 2; i++) {
    enabled = !enabled;
    UrlbarPrefs.set("quicksuggest.dataCollection.enabled", enabled);
    TelemetryTestUtils.assertEvents([
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: enabled ? "enabled" : "disabled",
      },
    ]);
    Assert.equal(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        "browser.urlbar.quicksuggest.dataCollection.enabled"
      ],
      enabled,
      "quicksuggest.dataCollection.enabled is correct in TelemetryEnvironment"
    );
  }

  // Set the main quicksuggest.enabled pref to false and toggle the data
  // collection pref again. We shouldn't get any events.
  await SpecialPowers.pushPrefEnv({
    set: [[EXPERIMENT_PREF, false]],
  });
  enabled = !enabled;
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", enabled);
  TelemetryTestUtils.assertEvents([], {
    category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
  });
  await SpecialPowers.popPrefEnv();

  // Set the pref back to what it was at the start of the task.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", !enabled);
});

// Tests telemetry recorded when clicking the checkbox for best match in
// preferences UI. The telemetry will be stored as following keyed scalar.
// scalar: browser.ui.interaction.preferences_panePrivacy
// key:    firefoxSuggestBestMatch
add_task(async function bestmatchCheckbox() {
  // Set the initial enabled status.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", true]],
  });

  // Open preferences page for best match.
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#privacy",
    true
  );

  for (let i = 0; i < 2; i++) {
    Services.telemetry.clearScalars();

    // Click on the checkbox.
    const doc = gBrowser.selectedBrowser.contentDocument;
    const checkboxId = "firefoxSuggestBestMatch";
    const checkbox = doc.getElementById(checkboxId);
    checkbox.scrollIntoView();
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#" + checkboxId,
      {},
      gBrowser.selectedBrowser
    );

    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "browser.ui.interaction.preferences_panePrivacy",
      checkboxId,
      1
    );
  }

  // Clean up.
  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Tests telemetry recorded when opening the learn more link for best match in
// the preferences UI. The telemetry will be stored as following keyed scalar.
// scalar: browser.ui.interaction.preferences_panePrivacy
// key:    firefoxSuggestBestMatchLearnMore
add_task(async function bestmatchLearnMore() {
  // Set the initial enabled status.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", true]],
  });

  // Open preferences page for best match.
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#privacy",
    true
  );

  // Click on the learn more link.
  Services.telemetry.clearScalars();
  const learnMoreLinkId = "firefoxSuggestBestMatchLearnMore";
  const doc = gBrowser.selectedBrowser.contentDocument;
  const link = doc.getElementById(learnMoreLinkId);
  link.scrollIntoView();
  const onLearnMoreOpenedByClick = BrowserTestUtils.waitForNewTab(
    gBrowser,
    QuickSuggestTestUtils.BEST_MATCH_LEARN_MORE_URL
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + learnMoreLinkId,
    {},
    gBrowser.selectedBrowser
  );
  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.ui.interaction.preferences_panePrivacy",
    "firefoxSuggestBestMatchLearnMore",
    1
  );
  await onLearnMoreOpenedByClick;
  gBrowser.removeCurrentTab();

  // Type enter key on the learm more link.
  Services.telemetry.clearScalars();
  link.focus();
  const onLearnMoreOpenedByKey = BrowserTestUtils.waitForNewTab(
    gBrowser,
    QuickSuggestTestUtils.BEST_MATCH_LEARN_MORE_URL
  );
  await BrowserTestUtils.synthesizeKey(
    "KEY_Enter",
    {},
    gBrowser.selectedBrowser
  );
  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.ui.interaction.preferences_panePrivacy",
    "firefoxSuggestBestMatchLearnMore",
    1
  );
  await onLearnMoreOpenedByKey;
  gBrowser.removeCurrentTab();

  // Clean up.
  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Tests the Nimbus exposure event gets recorded after a quick suggest result
// impression.
add_task(async function nimbusExposure() {
  await QuickSuggestTestUtils.clearExposureEvent();

  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestEnabled: true,
      quickSuggestShouldShowOnboardingDialog: false,
    },
    callback: async () => {
      // No exposure event should be recorded after only enrolling.
      await QuickSuggestTestUtils.assertExposureEvent(false);

      // Do a search that doesn't trigger a quick suggest result. No exposure
      // event should be recorded.
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "nimbusExposure no result",
        fireInputEvent: true,
      });
      await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
      await UrlbarTestUtils.promisePopupClose(window);
      QuickSuggestTestUtils.assertExposureEvent(false);

      // Do a search that does trigger a quick suggest result. The exposure
      // event should be recorded.
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: TEST_SEARCH_STRING,
        fireInputEvent: true,
      });
      await QuickSuggestTestUtils.assertIsQuickSuggest({
        window,
        index: 1,
        url: TEST_URL,
      });
      await QuickSuggestTestUtils.assertExposureEvent(true, "control");

      await UrlbarTestUtils.promisePopupClose(window);
    },
  });
});

// The "firefox-suggest-update" notification should cause TelemetryEnvironment
// to re-cache the `suggest.quicksuggest` prefs.
add_task(async function telemetryEnvironmentUpdateNotification() {
  // Make sure the prefs don't have user values that would mask the default
  // values set below.
  await QuickSuggestTestUtils.setScenario(null);
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");

  // Check the initial defaults.
  let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
  Assert.ok(
    defaults.getBoolPref("suggest.quicksuggest.nonsponsored"),
    "suggest.quicksuggest.nonsponsored is true initially"
  );
  Assert.ok(
    defaults.getBoolPref("suggest.quicksuggest.sponsored"),
    "suggest.quicksuggest.sponsored is true initially"
  );

  // Tell TelemetryEnvironment to clear its pref cache and stop observing prefs.
  await TelemetryEnvironment.testWatchPreferences(new Map());

  // Set the prefs to false. They should remain absent in TelemetryEnvironment.
  defaults.setBoolPref("suggest.quicksuggest.nonsponsored", false);
  defaults.setBoolPref("suggest.quicksuggest.sponsored", false);
  Assert.ok(
    !(
      "browser.urlbar.suggest.quicksuggest.nonsponsored" in
      TelemetryEnvironment.currentEnvironment.settings.userPrefs
    ),
    "suggest.quicksuggest.nonsponsored not in TelemetryEnvironment"
  );
  Assert.ok(
    !(
      "browser.urlbar.suggest.quicksuggest.sponsored" in
      TelemetryEnvironment.currentEnvironment.settings.userPrefs
    ),
    "suggest.quicksuggest.sponsored not in TelemetryEnvironment"
  );

  // Send the notification. TelemetryEnvironment should record the current
  // values.
  Services.obs.notifyObservers(null, "firefox-suggest-update");
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ],
    false,
    "suggest.quicksuggest.nonsponsored is false in TelemetryEnvironment"
  );
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ],
    false,
    "suggest.quicksuggest.sponsored is false in TelemetryEnvironment"
  );

  // Set the prefs to true. TelemetryEnvironment should keep the old values.
  defaults.setBoolPref("suggest.quicksuggest.nonsponsored", true);
  defaults.setBoolPref("suggest.quicksuggest.sponsored", true);
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ],
    false,
    "suggest.quicksuggest.nonsponsored remains false in TelemetryEnvironment"
  );
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ],
    false,
    "suggest.quicksuggest.sponsored remains false in TelemetryEnvironment"
  );

  // Send the notification again. TelemetryEnvironment should record the new
  // values.
  Services.obs.notifyObservers(null, "firefox-suggest-update");
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ],
    true,
    "suggest.quicksuggest.nonsponsored is false in TelemetryEnvironment"
  );
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ],
    true,
    "suggest.quicksuggest.sponsored is false in TelemetryEnvironment"
  );

  await TelemetryEnvironment.testCleanRestart().onInitialized();
});

/**
 * Adds a search engine that provides suggestions, calls your callback, and then
 * removes the engine.
 *
 * @param {function} callback
 *   Your callback function.
 */
async function withSuggestions(callback) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  try {
    await callback(engine);
  } finally {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
    await SpecialPowers.popPrefEnv();
  }
}
