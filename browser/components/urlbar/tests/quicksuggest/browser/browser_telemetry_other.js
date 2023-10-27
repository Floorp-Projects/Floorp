/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests ancillary quick suggest telemetry, i.e., telemetry that's not
 * strongly related to showing suggestions in the urlbar.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
});

const REMOTE_SETTINGS_RESULTS = [
  {
    id: 1,
    url: "https://example.com/sponsored",
    title: "Sponsored suggestion",
    keywords: ["sponsored"],
    click_url: "https://example.com/click",
    impression_url: "https://example.com/impression",
    advertiser: "testadvertiser",
  },
];

add_setup(async function () {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: REMOTE_SETTINGS_RESULTS,
      },
    ],
  });
});

// Tests telemetry recorded when toggling the
// `suggest.quicksuggest.nonsponsored` pref:
// * contextservices.quicksuggest enable_toggled event telemetry
// * TelemetryEnvironment
add_task(async function enableToggled() {
  Services.telemetry.clearEvents();

  // Toggle the suggest.quicksuggest.nonsponsored pref twice. We should get two
  // events.
  let enabled = UrlbarPrefs.get("suggest.quicksuggest.nonsponsored");
  for (let i = 0; i < 2; i++) {
    enabled = !enabled;
    UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", enabled);
    QuickSuggestTestUtils.assertEvents([
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
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
    set: [["browser.urlbar.quicksuggest.enabled", false]],
  });
  enabled = !enabled;
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", enabled);
  QuickSuggestTestUtils.assertEvents([]);
  await SpecialPowers.popPrefEnv();

  // Set the pref back to what it was at the start of the task.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", !enabled);
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
    QuickSuggestTestUtils.assertEvents([
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
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
    set: [["browser.urlbar.quicksuggest.enabled", false]],
  });
  enabled = !enabled;
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", enabled);
  QuickSuggestTestUtils.assertEvents([]);
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
    QuickSuggestTestUtils.assertEvents([
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
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
    set: [["browser.urlbar.quicksuggest.enabled", false]],
  });
  enabled = !enabled;
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", enabled);
  QuickSuggestTestUtils.assertEvents([]);
  await SpecialPowers.popPrefEnv();

  // Set the pref back to what it was at the start of the task.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", !enabled);
});

// Simulates the race on startup between telemetry environment initialization
// and the initial update of the Suggest scenario. After startup is done,
// telemetry environment should record the correct values for startup prefs.
add_task(async function telemetryEnvironmentOnStartup() {
  await QuickSuggestTestUtils.setScenario(null);

  // Restart telemetry environment so we know it's watching its default set of
  // prefs.
  await TelemetryEnvironment.testCleanRestart().onInitialized();

  // Get the prefs that UrlbarPrefs sets when the Suggest scenario is updated on
  // startup. They're the union of the prefs exposed in the UI and the prefs
  // that are set on the default branch per scenario.
  let prefs = [
    ...new Set([
      ...Object.values(UrlbarPrefs.FIREFOX_SUGGEST_UI_PREFS_BY_VARIABLE),
      ...Object.values(UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS)
        .map(valuesByPrefName => Object.keys(valuesByPrefName))
        .flat(),
    ]),
  ];

  // Not all of the prefs are recorded in telemetry environment. Filter in the
  // ones that are.
  prefs = prefs.filter(
    p =>
      `browser.urlbar.${p}` in
      TelemetryEnvironment.currentEnvironment.settings.userPrefs
  );

  info("Got startup prefs: " + JSON.stringify(prefs));

  // Sanity check the expected prefs. This isn't strictly necessary since we
  // programmatically get the prefs above, but it's an extra layer of defense,
  // for example in case we accidentally filtered out some expected prefs above.
  // If this fails, you might have added a startup pref but didn't update this
  // array here.
  Assert.deepEqual(
    prefs.sort(),
    [
      "quicksuggest.dataCollection.enabled",
      "suggest.quicksuggest.nonsponsored",
      "suggest.quicksuggest.sponsored",
    ],
    "Expected startup prefs"
  );

  // Make sure the prefs don't have user values that would mask the default
  // values.
  for (let p of prefs) {
    UrlbarPrefs.clear(p);
  }

  // Build a map of default values.
  let defaultValues = Object.fromEntries(
    prefs.map(p => [p, UrlbarPrefs.get(p)])
  );

  // Now simulate startup. Restart telemetry environment but don't wait for it
  // to finish before calling `updateFirefoxSuggestScenario()`. This simulates
  // startup where telemetry environment's initialization races the intial
  // update of the Suggest scenario.
  let environmentInitPromise =
    TelemetryEnvironment.testCleanRestart().onInitialized();

  // Update the scenario and force the startup prefs to take on values that are
  // the inverse of what they are now.
  await UrlbarPrefs.updateFirefoxSuggestScenario({
    isStartup: true,
    scenario: "online",
    defaultPrefs: {
      online: Object.fromEntries(
        Object.entries(defaultValues).map(([p, value]) => [p, !value])
      ),
    },
  });

  // At this point telemetry environment should be done initializing since
  // `updateFirefoxSuggestScenario()` waits for it, but await our promise now.
  await environmentInitPromise;

  // TelemetryEnvironment should have cached the new values.
  for (let [p, value] of Object.entries(defaultValues)) {
    let expected = !value;
    Assert.strictEqual(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        `browser.urlbar.${p}`
      ],
      expected,
      `Check 1: ${p} is ${expected} in TelemetryEnvironment`
    );
  }

  // Simulate another startup and set all prefs back to their original default
  // values.
  environmentInitPromise =
    TelemetryEnvironment.testCleanRestart().onInitialized();

  await UrlbarPrefs.updateFirefoxSuggestScenario({
    isStartup: true,
    scenario: "online",
    defaultPrefs: {
      online: defaultValues,
    },
  });

  await environmentInitPromise;

  // TelemetryEnvironment should have cached the new (original) values.
  for (let [p, value] of Object.entries(defaultValues)) {
    let expected = value;
    Assert.strictEqual(
      TelemetryEnvironment.currentEnvironment.settings.userPrefs[
        `browser.urlbar.${p}`
      ],
      expected,
      `Check 2: ${p} is ${expected} in TelemetryEnvironment`
    );
  }

  await TelemetryEnvironment.testCleanRestart().onInitialized();
});
