/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests urlbar telemetry for Quick Suggest results.
 * See also browser_quicksuggest_onboardingDialog.js for onboarding telemetry.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

const TEST_URL = "http://example.com/quicksuggest?q=frabbits";
const TEST_SEARCH_STRING = "frab";
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
    search_query: undefined,
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
    search_query: TEST_SEARCH_STRING,
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
    search_query: undefined,
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
    search_query: TEST_SEARCH_STRING,
  });
});

async function doImpressionTest({ scenario, search_query }) {
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
      search_query,
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
    search_query: undefined,
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
    search_query: TEST_SEARCH_STRING,
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
    search_query: undefined,
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
    search_query: TEST_SEARCH_STRING,
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
    search_query: undefined,
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
    search_query: TEST_SEARCH_STRING,
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
    search_query: undefined,
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
    search_query: TEST_SEARCH_STRING,
  });
});

async function doClickTest({ useKeyboard, scenario, search_query }) {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
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
    QuickSuggestTestUtils.assertImpressionPing({
      index,
      spy,
      scenario,
      search_query,
    });
    QuickSuggestTestUtils.assertClickPing({ index, spy, scenario });
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
      QuickSuggestTestUtils.assertImpressionPing({ index, spy });
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
  let helpButton = result.element.row._elements.get("helpButton");
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
  let helpButton = result.element.row._elements.get("helpButton");
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

// Tests the Nimbus exposure event gets recorded after a quick suggest result
// impression.
add_task(async function nimbusExposure() {
  // Exposure event recording is queued to the idle thread, so wait for idle
  // before we start so any events from previous tasks will have been recorded
  // and won't interfere with this task.
  await new Promise(resolve => Services.tm.idleDispatchToMainThread(resolve));

  Services.telemetry.clearEvents();
  NimbusFeatures.urlbar._didSendExposureEvent = false;
  UrlbarProviderQuickSuggest._recordedExposureEvent = false;
  let doExperimentCleanup = await QuickSuggestTestUtils.enrollExperiment({
    valueOverrides: {
      quickSuggestEnabled: true,
      quickSuggestShouldShowOnboardingDialog: false,
    },
  });

  // This filter is needed to exclude the enrollment event.
  let filter = {
    category: "normandy",
    method: "expose",
    object: "nimbus_experiment",
  };

  // No exposure event should be recorded after only enrolling.
  Assert.ok(
    !UrlbarProviderQuickSuggest._recordedExposureEvent,
    "_recordedExposureEvent remains false after enrolling"
  );
  TelemetryTestUtils.assertEvents([], filter);

  // Do a search that doesn't trigger a quick suggest result. No exposure event
  // should be recorded.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "nimbusExposure no result",
    fireInputEvent: true,
  });
  await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
  await UrlbarTestUtils.promisePopupClose(window);
  Assert.ok(
    !UrlbarProviderQuickSuggest._recordedExposureEvent,
    "_recordedExposureEvent remains false after no quick suggest result"
  );
  TelemetryTestUtils.assertEvents([], filter);

  // Do a search that does trigger a quick suggest result. The exposure event
  // should be recorded.
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
  Assert.ok(
    UrlbarProviderQuickSuggest._recordedExposureEvent,
    "_recordedExposureEvent is true after showing quick suggest result"
  );

  // The event recording is queued to the idle thread when the search starts, so
  // likewise queue the assert to idle instead of doing it immediately.
  await new Promise(resolve => {
    Services.tm.idleDispatchToMainThread(() => {
      TelemetryTestUtils.assertEvents(
        [
          {
            category: "normandy",
            method: "expose",
            object: "nimbus_experiment",
            extra: {
              branchSlug: "control",
              featureId: "urlbar",
            },
          },
        ],
        filter
      );
      resolve();
    });
  });

  await UrlbarTestUtils.promisePopupClose(window);

  await doExperimentCleanup();
});

// The contextservices.quicksuggest enable_toggled and sponsored_toggled events
// should not be recorded when the scenario changes. TelemetryEnvironment should
// record the new `suggest.quicksuggest` pref values.
add_task(async function updateScenario() {
  // Make sure the prefs don't have user values that would mask the default
  // values set below.
  await QuickSuggestTestUtils.setScenario(null);
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  Services.telemetry.clearEvents();

  // check initial defaults
  let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.scenario"),
    "offline",
    "Default scenario is offline initially"
  );
  Assert.ok(
    defaults.getBoolPref("suggest.quicksuggest.nonsponsored"),
    "suggest.quicksuggest.nonsponsored is true initially"
  );
  Assert.ok(
    defaults.getBoolPref("suggest.quicksuggest.sponsored"),
    "suggest.quicksuggest.sponsored is true initially"
  );
  Assert.ok(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ],
    "suggest.quicksuggest.nonsponsored is true in TelemetryEnvironment"
  );
  Assert.ok(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ],
    "suggest.quicksuggest.sponsored is true in TelemetryEnvironment"
  );

  // set online
  await QuickSuggestTestUtils.setScenario("online");
  Assert.ok(
    !defaults.getBoolPref("suggest.quicksuggest.nonsponsored"),
    "suggest.quicksuggest.nonsponsored is false after setting online scenario"
  );
  Assert.ok(
    !defaults.getBoolPref("suggest.quicksuggest.sponsored"),
    "suggest.quicksuggest.sponsored is false after setting online scenario"
  );
  TelemetryTestUtils.assertEvents([]);
  Assert.ok(
    !TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ],
    "suggest.quicksuggest.nonsponsored is false in TelemetryEnvironment"
  );
  Assert.ok(
    !TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ],
    "suggest.quicksuggest.sponsored is false in TelemetryEnvironment"
  );

  // set back to offline
  await QuickSuggestTestUtils.setScenario("offline");
  Assert.ok(
    defaults.getBoolPref("suggest.quicksuggest.nonsponsored"),
    "suggest.quicksuggest.nonsponsored is true after setting offline again"
  );
  Assert.ok(
    defaults.getBoolPref("suggest.quicksuggest.sponsored"),
    "suggest.quicksuggest.sponsored is true after setting offline again"
  );
  TelemetryTestUtils.assertEvents([]);
  Assert.ok(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ],
    "suggest.quicksuggest.nonsponsored is true in TelemetryEnvironment again"
  );
  Assert.ok(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ],
    "suggest.quicksuggest.sponsored is true in TelemetryEnvironment again"
  );
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
