/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests QuickSuggest configurations.
 */

ChromeUtils.defineESModuleGetters(this, {
  EnterprisePolicyTesting:
    "resource://testing-common/EnterprisePolicyTesting.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

// We use this pref in enterprise preference policy tests. We specifically use a
// pref that's sticky and exposed in the UI to make sure it can be set properly.
const POLICY_PREF = "suggest.quicksuggest.nonsponsored";

let gDefaultBranch = Services.prefs.getDefaultBranch("browser.urlbar.");
let gUserBranch = Services.prefs.getBranch("browser.urlbar.");

add_setup(async function () {
  await QuickSuggestTestUtils.ensureQuickSuggestInit();
});

// Makes sure `QuickSuggest._updateFeatureState()` is called when the
// `browser.urlbar.quicksuggest.enabled` pref is changed.
add_task(async function test_updateFeatureState_pref() {
  Assert.ok(
    UrlbarPrefs.get("quicksuggest.enabled"),
    "Sanity check: quicksuggest.enabled is true by default"
  );

  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(QuickSuggest, "_updateFeatureState");

  UrlbarPrefs.set("quicksuggest.enabled", false);
  Assert.equal(
    spy.callCount,
    1,
    "_updateFeatureState called once after changing pref"
  );

  UrlbarPrefs.clear("quicksuggest.enabled");
  Assert.equal(
    spy.callCount,
    2,
    "_updateFeatureState called again after clearing pref"
  );

  sandbox.restore();
});

// Makes sure `QuickSuggest._updateFeatureState()` is called when a Nimbus
// experiment is installed and uninstalled.
add_task(async function test_updateFeatureState_experiment() {
  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(QuickSuggest, "_updateFeatureState");

  await QuickSuggestTestUtils.withExperiment({
    callback: () => {
      Assert.equal(
        spy.callCount,
        1,
        "_updateFeatureState called once after installing experiment"
      );
    },
  });

  Assert.equal(
    spy.callCount,
    2,
    "_updateFeatureState called again after uninstalling experiment"
  );

  sandbox.restore();
});

add_task(async function test_indexes() {
  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestNonSponsoredIndex: 99,
      quickSuggestSponsoredIndex: -1337,
    },
    callback: () => {
      Assert.equal(
        UrlbarPrefs.get("quickSuggestNonSponsoredIndex"),
        99,
        "quickSuggestNonSponsoredIndex"
      );
      Assert.equal(
        UrlbarPrefs.get("quickSuggestSponsoredIndex"),
        -1337,
        "quickSuggestSponsoredIndex"
      );
    },
  });
});

add_task(async function test_merino() {
  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      merinoEndpointURL: "http://example.com/test_merino_config",
      merinoClientVariants: "test-client-variants",
      merinoProviders: "test-providers",
    },
    callback: () => {
      Assert.equal(
        UrlbarPrefs.get("merinoEndpointURL"),
        "http://example.com/test_merino_config",
        "merinoEndpointURL"
      );
      Assert.equal(
        UrlbarPrefs.get("merinoClientVariants"),
        "test-client-variants",
        "merinoClientVariants"
      );
      Assert.equal(
        UrlbarPrefs.get("merinoProviders"),
        "test-providers",
        "merinoProviders"
      );
    },
  });
});

add_task(async function test_scenario_online() {
  await doBasicScenarioTest("online", {
    urlbarPrefs: {
      // prefs
      "quicksuggest.scenario": "online",
      "quicksuggest.enabled": true,
      "quicksuggest.dataCollection.enabled": false,
      "quicksuggest.shouldShowOnboardingDialog": true,
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,

      // Nimbus variables
      quickSuggestScenario: "online",
      quickSuggestEnabled: true,
      quickSuggestShouldShowOnboardingDialog: true,
    },
    defaults: [
      {
        name: "browser.urlbar.quicksuggest.enabled",
        value: true,
      },
      {
        name: "browser.urlbar.quicksuggest.dataCollection.enabled",
        value: false,
      },
      {
        name: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
        value: true,
      },
      {
        name: "browser.urlbar.suggest.quicksuggest.nonsponsored",
        value: true,
      },
      {
        name: "browser.urlbar.suggest.quicksuggest.sponsored",
        value: true,
      },
    ],
  });
});

add_task(async function test_scenario_offline() {
  await doBasicScenarioTest("offline", {
    urlbarPrefs: {
      // prefs
      "quicksuggest.scenario": "offline",
      "quicksuggest.enabled": true,
      "quicksuggest.dataCollection.enabled": false,
      "quicksuggest.shouldShowOnboardingDialog": false,
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,

      // Nimbus variables
      quickSuggestScenario: "offline",
      quickSuggestEnabled: true,
      quickSuggestShouldShowOnboardingDialog: false,
    },
    defaults: [
      {
        name: "browser.urlbar.quicksuggest.enabled",
        value: true,
      },
      {
        name: "browser.urlbar.quicksuggest.dataCollection.enabled",
        value: false,
      },
      {
        name: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
        value: false,
      },
      {
        name: "browser.urlbar.suggest.quicksuggest.nonsponsored",
        value: true,
      },
      {
        name: "browser.urlbar.suggest.quicksuggest.sponsored",
        value: true,
      },
    ],
  });
});

add_task(async function test_scenario_history() {
  await doBasicScenarioTest("history", {
    urlbarPrefs: {
      // prefs
      "quicksuggest.scenario": "history",
      "quicksuggest.enabled": false,

      // Nimbus variables
      quickSuggestScenario: "history",
      quickSuggestEnabled: false,
    },
    defaults: [
      {
        name: "browser.urlbar.quicksuggest.enabled",
        value: false,
      },
    ],
  });
});

async function doBasicScenarioTest(scenario, expectedPrefs) {
  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: scenario,
    },
    callback: () => {
      // Pref updates should always settle down by the time enrollment is done.
      Assert.ok(
        !UrlbarPrefs.updatingFirefoxSuggestPrefs,
        "updatingFirefoxSuggestPrefs is false"
      );

      assertScenarioPrefs(expectedPrefs);
    },
  });

  // Similarly, pref updates should always settle down by the time unenrollment
  // is done.
  Assert.ok(
    !UrlbarPrefs.updatingFirefoxSuggestPrefs,
    "updatingFirefoxSuggestPrefs is false"
  );

  assertDefaultScenarioPrefs();
}

function assertScenarioPrefs({ urlbarPrefs, defaults }) {
  for (let [name, value] of Object.entries(urlbarPrefs)) {
    Assert.equal(UrlbarPrefs.get(name), value, `UrlbarPrefs.get("${name}")`);
  }

  let prefs = Services.prefs.getDefaultBranch("");
  for (let { name, getter, value } of defaults) {
    Assert.equal(
      prefs[getter || "getBoolPref"](name),
      value,
      `Default branch pref: ${name}`
    );
  }
}

function assertDefaultScenarioPrefs() {
  assertScenarioPrefs({
    urlbarPrefs: {
      "quicksuggest.scenario": "offline",
      "quicksuggest.enabled": true,
      "quicksuggest.dataCollection.enabled": false,
      "quicksuggest.shouldShowOnboardingDialog": false,
      "suggest.quicksuggest.nonsponsored": true,
      "suggest.quicksuggest.sponsored": true,

      // No Nimbus variables since they're only available when an experiment is
      // installed.
    },
    defaults: [
      {
        name: "browser.urlbar.quicksuggest.enabled",
        value: true,
      },
      {
        name: "browser.urlbar.quicksuggest.dataCollection.enabled",
        value: false,
      },
      {
        name: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
        value: false,
      },
      {
        name: "browser.urlbar.suggest.quicksuggest.nonsponsored",
        value: true,
      },
      {
        name: "browser.urlbar.suggest.quicksuggest.sponsored",
        value: true,
      },
    ],
  });
}

function clearOnboardingPrefs() {
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
  UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.seenRestarts");
}

// The following tasks test Nimbus enrollments

// Initial state:
// * History (quick suggest feature disabled)
//
// Enrollment:
// * History
//
// Expected:
// * All history prefs set on the default branch
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
    valueOverrides: {
      quickSuggestScenario: "history",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
  });
});

// Initial state:
// * History (quick suggest feature disabled)
//
// Enrollment:
// * Offline
//
// Expected:
// * All offline prefs set on the default branch
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
  });
});

// Initial state:
// * History (quick suggest feature disabled)
//
// Enrollment:
// * Online
//
// Expected:
// * All online prefs set on the default branch
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.history,
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
  });
});

// The following tasks test OFFLINE TO OFFLINE

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * User did not override any defaults
//
// Enrollment:
// * Offline
//
// Expected:
// * All offline prefs set on the default branch
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Enrollment:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user left on
// * Data collection: user left off
//
// Enrollment:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain on
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Enrollment:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain off
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user turned on
//
// Enrollment:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user turned on
//
// Enrollment:
// * Offline
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain off
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// The following tasks test OFFLINE TO ONLINE

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * User did not override any defaults
//
// Enrollment:
// * Online
//
// Expected:
// * All online prefs set on the default branch
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user left on
// * Data collection: user left off
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain on
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain off
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user left on
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain on
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain off
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// The following tasks test ONLINE TO ONLINE

// Initial state:
// * Online (suggestions on and data collection off by default)
// * User did not override any defaults
//
// Enrollment:
// * Online
//
// Expected:
// * All online prefs set on the default branch
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user left on
// * Data collection: user left off
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain on
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user left off
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain off
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user left on
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain on
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user left on
// * Sponsored suggestions: user turned off
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain on
// * Sponsored suggestions: remain off
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user left on
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain on
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Non-sponsored suggestions: user turned off
// * Sponsored suggestions: user turned off
// * Data collection: user turned on
//
// Enrollment:
// * Online
//
// Expected:
// * Non-sponsored suggestions: remain off
// * Sponsored suggestions: remain off
// * Data collection: remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    expectedPrefs: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// The following tasks test scenarios in conjunction with individual Nimbus
// variables

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * User did not override any defaults
//
// Enrollment:
// * Offline
// * Sponsored suggestions individually forced on
//
// Expected:
// * Sponsored suggestions: on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
      quickSuggestSponsoredEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Sponsored suggestions: user turned off
//
// Enrollment:
// * Offline
// * Sponsored suggestions individually forced on
//
// Expected:
// * Sponsored suggestions: remain off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
      quickSuggestSponsoredEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
        "suggest.quicksuggest.sponsored": true,
      },
      userBranch: {
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * User did not override any defaults
//
// Enrollment:
// * Offline
// * Data collection individually forced on
//
// Expected:
// * Data collection: on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
      quickSuggestDataCollectionEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Offline (suggestions on and data collection off by default)
// * Data collection: user turned off (it's off by default, so this simulates
//   when the user toggled it on and then back off)
//
// Enrollment:
// * Offline
// * Data collection individually forced on
//
// Expected:
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
      userBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "offline",
      quickSuggestDataCollectionEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline,
        "quicksuggest.dataCollection.enabled": true,
      },
      userBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * User did not override any defaults
//
// Enrollment:
// * Online
// * Sponsored suggestions individually forced off
//
// Expected:
// * Sponsored suggestions: off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestSponsoredEnabled: false,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Sponsored suggestions: user turned on (they're on by default, so this
//   simulates when the user toggled them off and then back on)
//
// Enrollment:
// * Online
// * Sponsored suggestions individually forced off
//
// Expected:
// * Sponsored suggestions: remain on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "suggest.quicksuggest.sponsored": true,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestSponsoredEnabled: false,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
        "suggest.quicksuggest.sponsored": false,
      },
      userBranch: {
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * User did not override any defaults
//
// Enrollment:
// * Online
// * Data collection individually forced on
//
// Expected:
// * Data collection: on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
    },
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestDataCollectionEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

// Initial state:
// * Online (suggestions on and data collection off by default)
// * Data collection: user turned off (it's off by default, so this simulates
//   when the user toggled it on and then back off)
//
// Enrollment:
// * Online
// * Data collection individually forced on
//
// Expected:
// * Data collection: remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
      userBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
    },
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestDataCollectionEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        ...UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.online,
        "quicksuggest.dataCollection.enabled": true,
      },
      userBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
    },
  });
});

// The following tasks test individual Nimbus variables without scenarios

// Initial state:
// * Suggestions on by default and user left them on
//
// 1. First enrollment:
//    * Suggestions forced off
//
//    Expected:
//    * Suggestions off
//
// 2. User turns on suggestions
// 3. Second enrollment:
//    * Suggestions forced off again
//
//    Expected:
//    * Suggestions remain on
add_task(async function () {
  await checkEnrollments([
    {
      initialPrefsToSet: {
        defaultBranch: {
          "suggest.quicksuggest.nonsponsored": true,
          "suggest.quicksuggest.sponsored": true,
        },
      },
      valueOverrides: {
        quickSuggestNonSponsoredEnabled: false,
        quickSuggestSponsoredEnabled: false,
      },
      expectedPrefs: {
        defaultBranch: {
          "suggest.quicksuggest.nonsponsored": false,
          "suggest.quicksuggest.sponsored": false,
        },
      },
    },
    {
      initialPrefsToSet: {
        userBranch: {
          "suggest.quicksuggest.nonsponsored": true,
          "suggest.quicksuggest.sponsored": true,
        },
      },
      valueOverrides: {
        quickSuggestNonSponsoredEnabled: false,
        quickSuggestSponsoredEnabled: false,
      },
      expectedPrefs: {
        defaultBranch: {
          "suggest.quicksuggest.nonsponsored": false,
          "suggest.quicksuggest.sponsored": false,
        },
        userBranch: {
          "suggest.quicksuggest.nonsponsored": true,
          "suggest.quicksuggest.sponsored": true,
        },
      },
    },
  ]);
});

// Initial state:
// * Suggestions on by default but user turned them off
//
// Enrollment:
// * Suggestions forced on
//
// Expected:
// * Suggestions remain off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
    valueOverrides: {
      quickSuggestNonSponsoredEnabled: true,
      quickSuggestSponsoredEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
      userBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
    },
  });
});

// Initial state:
// * Suggestions off by default and user left them off
//
// 1. First enrollment:
//    * Suggestions forced on
//
//    Expected:
//    * Suggestions on
//
// 2. User turns off suggestions
// 3. Second enrollment:
//    * Suggestions forced on again
//
//    Expected:
//    * Suggestions remain off
add_task(async function () {
  await checkEnrollments([
    {
      initialPrefsToSet: {
        defaultBranch: {
          "suggest.quicksuggest.nonsponsored": false,
          "suggest.quicksuggest.sponsored": false,
        },
      },
      valueOverrides: {
        quickSuggestNonSponsoredEnabled: true,
        quickSuggestSponsoredEnabled: true,
      },
      expectedPrefs: {
        defaultBranch: {
          "suggest.quicksuggest.nonsponsored": true,
          "suggest.quicksuggest.sponsored": true,
        },
      },
    },
    {
      initialPrefsToSet: {
        userBranch: {
          "suggest.quicksuggest.nonsponsored": false,
          "suggest.quicksuggest.sponsored": false,
        },
      },
      valueOverrides: {
        quickSuggestNonSponsoredEnabled: true,
        quickSuggestSponsoredEnabled: true,
      },
      expectedPrefs: {
        defaultBranch: {
          "suggest.quicksuggest.nonsponsored": true,
          "suggest.quicksuggest.sponsored": true,
        },
        userBranch: {
          "suggest.quicksuggest.nonsponsored": false,
          "suggest.quicksuggest.sponsored": false,
        },
      },
    },
  ]);
});

// Initial state:
// * Suggestions off by default but user turned them on
//
// Enrollment:
// * Suggestions forced off
//
// Expected:
// * Suggestions remain on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
    },
    valueOverrides: {
      quickSuggestNonSponsoredEnabled: false,
      quickSuggestSponsoredEnabled: false,
    },
    expectedPrefs: {
      defaultBranch: {
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
      userBranch: {
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
    },
  });
});

// Initial state:
// * Data collection on by default and user left them on
//
// 1. First enrollment:
//    * Data collection forced off
//
//    Expected:
//    * Data collection off
//
// 2. User turns on data collection
// 3. Second enrollment:
//    * Data collection forced off again
//
//    Expected:
//    * Data collection remains on
add_task(async function () {
  await checkEnrollments(
    [
      {
        initialPrefsToSet: {
          defaultBranch: {
            "quicksuggest.dataCollection.enabled": true,
          },
        },
        valueOverrides: {
          quickSuggestDataCollectionEnabled: false,
        },
        expectedPrefs: {
          defaultBranch: {
            "quicksuggest.dataCollection.enabled": false,
          },
        },
      },
    ],
    [
      {
        initialPrefsToSet: {
          userBranch: {
            "quicksuggest.dataCollection.enabled": true,
          },
        },
        valueOverrides: {
          quickSuggestDataCollectionEnabled: false,
        },
        expectedPrefs: {
          defaultBranch: {
            "quicksuggest.dataCollection.enabled": false,
          },
          userBranch: {
            "quicksuggest.dataCollection.enabled": true,
          },
        },
      },
    ]
  );
});

// Initial state:
// * Data collection on by default but user turned it off
//
// Enrollment:
// * Data collection forced on
//
// Expected:
// * Data collection remains off
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
      userBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
    },
    valueOverrides: {
      quickSuggestDataCollectionEnabled: true,
    },
    expectedPrefs: {
      defaultBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
      userBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
    },
  });
});

// Initial state:
// * Data collection off by default and user left it off
//
// 1. First enrollment:
//    * Data collection forced on
//
//    Expected:
//    * Data collection on
//
// 2. User turns off data collection
// 3. Second enrollment:
//    * Data collection forced on again
//
//    Expected:
//    * Data collection remains off
add_task(async function () {
  await checkEnrollments(
    [
      {
        initialPrefsToSet: {
          defaultBranch: {
            "quicksuggest.dataCollection.enabled": false,
          },
        },
        valueOverrides: {
          quickSuggestDataCollectionEnabled: true,
        },
        expectedPrefs: {
          defaultBranch: {
            "quicksuggest.dataCollection.enabled": true,
          },
        },
      },
    ],
    [
      {
        initialPrefsToSet: {
          userBranch: {
            "quicksuggest.dataCollection.enabled": false,
          },
        },
        valueOverrides: {
          quickSuggestDataCollectionEnabled: true,
        },
        expectedPrefs: {
          defaultBranch: {
            "quicksuggest.dataCollection.enabled": false,
          },
          userBranch: {
            "quicksuggest.dataCollection.enabled": false,
          },
        },
      },
    ]
  );
});

// Initial state:
// * Data collection off by default but user turned it on
//
// Enrollment:
// * Data collection forced off
//
// Expected:
// * Data collection remains on
add_task(async function () {
  await checkEnrollments({
    initialPrefsToSet: {
      defaultBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
    valueOverrides: {
      quickSuggestDataCollectionEnabled: false,
    },
    expectedPrefs: {
      defaultBranch: {
        "quicksuggest.dataCollection.enabled": false,
      },
      userBranch: {
        "quicksuggest.dataCollection.enabled": true,
      },
    },
  });
});

/**
 * Tests one or more enrollments. Sets an initial set of prefs on the default
 * and/or user branches, enrolls in a mock Nimbus experiment, checks expected
 * pref values, unenrolls, and finally checks prefs again.
 *
 * The given `options` value may be an object as described below or an array of
 * such objects, one per enrollment.
 *
 * @param {object} options
 *   Function options.
 * @param {object} options.initialPrefsToSet
 *   An object: { userBranch, defaultBranch }
 *   `userBranch` and `defaultBranch` are objects that map pref names (relative
 *   to `browser.urlbar`) to values. These prefs will be set on the appropriate
 *   branch before enrollment. Both `userBranch` and `defaultBranch` are
 *   optional.
 * @param {object} options.valueOverrides
 *   The `valueOverrides` object passed to the mock experiment. It should map
 *   Nimbus variable names to values.
 * @param {object} options.expectedPrefs
 *   Preferences that should be set after enrollment. It has the same shape as
 *   `options.initialPrefsToSet`.
 */
async function checkEnrollments(options) {
  info("Testing: " + JSON.stringify(options));

  let enrollments;
  if (Array.isArray(options)) {
    enrollments = options;
  } else {
    enrollments = [options];
  }

  // Do each enrollment.
  for (let i = 0; i < enrollments.length; i++) {
    info(
      `Starting setup for enrollment ${i}: ` + JSON.stringify(enrollments[i])
    );

    let { initialPrefsToSet, valueOverrides, expectedPrefs } = enrollments[i];

    // Set initial prefs.
    UrlbarPrefs._updatingFirefoxSuggestScenario = true;
    let { defaultBranch: initialDefaultBranch, userBranch: initialUserBranch } =
      initialPrefsToSet;
    initialDefaultBranch = initialDefaultBranch || {};
    initialUserBranch = initialUserBranch || {};
    for (let name of Object.keys(initialDefaultBranch)) {
      // Clear user-branch values on the default prefs so the defaults aren't
      // masked.
      gUserBranch.clearUserPref(name);
    }
    for (let [branch, prefs] of [
      [gDefaultBranch, initialDefaultBranch],
      [gUserBranch, initialUserBranch],
    ]) {
      for (let [name, value] of Object.entries(prefs)) {
        branch.setBoolPref(name, value);
      }
    }
    UrlbarPrefs._updatingFirefoxSuggestScenario = false;

    let {
      defaultBranch: expectedDefaultBranch,
      userBranch: expectedUserBranch,
    } = expectedPrefs;
    expectedDefaultBranch = expectedDefaultBranch || {};
    expectedUserBranch = expectedUserBranch || {};

    // Install the experiment.
    info(`Installing experiment for enrollment ${i}`);
    await QuickSuggestTestUtils.withExperiment({
      valueOverrides,
      callback: () => {
        info(`Installed experiment for enrollment ${i}, now checking prefs`);

        // Check expected pref values. Store expected effective values as we go
        // so we can check them afterward. For a given pref, the expected
        // effective value is the user value, or if there's not a user value,
        // the default value.
        let expectedEffectivePrefs = {};
        for (let [branch, prefs, branchType] of [
          [gDefaultBranch, expectedDefaultBranch, "default"],
          [gUserBranch, expectedUserBranch, "user"],
        ]) {
          for (let [name, value] of Object.entries(prefs)) {
            expectedEffectivePrefs[name] = value;
            Assert.equal(
              branch.getBoolPref(name),
              value,
              `Pref ${name} on ${branchType} branch`
            );
            if (branch == gUserBranch) {
              Assert.ok(
                gUserBranch.prefHasUserValue(name),
                `Pref ${name} is on user branch`
              );
            }
          }
        }
        for (let name of Object.keys(initialDefaultBranch)) {
          if (!expectedUserBranch.hasOwnProperty(name)) {
            Assert.ok(
              !gUserBranch.prefHasUserValue(name),
              `Pref ${name} is not on user branch`
            );
          }
        }
        for (let [name, value] of Object.entries(expectedEffectivePrefs)) {
          Assert.equal(
            UrlbarPrefs.get(name),
            value,
            `Pref ${name} effective value`
          );
        }

        info(`Uninstalling experiment for enrollment ${i}`);
      },
    });

    info(`Uninstalled experiment for enrollment ${i}, now checking prefs`);

    // Check expected effective values after unenrollment. The expected
    // effective value for a pref at this point is the value on the user branch,
    // or if there's not a user value, the original value on the default branch
    // before enrollment. This assumes the default values reflect the offline
    // scenario (the case for the U.S. region).
    let effectivePrefs = Object.assign(
      {},
      UrlbarPrefs.FIREFOX_SUGGEST_DEFAULT_PREFS.offline
    );
    for (let [name, value] of Object.entries(expectedUserBranch)) {
      effectivePrefs[name] = value;
    }
    for (let [name, value] of Object.entries(effectivePrefs)) {
      Assert.equal(
        UrlbarPrefs.get(name),
        value,
        `Pref ${name} effective value after unenrolling`
      );
    }

    // Clean up.
    UrlbarPrefs._updatingFirefoxSuggestScenario = true;
    for (let name of Object.keys(expectedUserBranch)) {
      UrlbarPrefs.clear(name);
    }
    UrlbarPrefs._updatingFirefoxSuggestScenario = false;
  }
}

// The following tasks test enterprise preference policies

// Preference policy test for the following:
// * Status: locked
// * Value: false
add_task(async function () {
  await doPolicyTest({
    prefPolicy: {
      Status: "locked",
      Value: false,
    },
    expectedDefault: false,
    expectedUser: undefined,
    expectedLocked: true,
  });
});

// Preference policy test for the following:
// * Status: locked
// * Value: true
add_task(async function () {
  await doPolicyTest({
    prefPolicy: {
      Status: "locked",
      Value: true,
    },
    expectedDefault: true,
    expectedUser: undefined,
    expectedLocked: true,
  });
});

// Preference policy test for the following:
// * Status: default
// * Value: false
add_task(async function () {
  await doPolicyTest({
    prefPolicy: {
      Status: "default",
      Value: false,
    },
    expectedDefault: false,
    expectedUser: undefined,
    expectedLocked: false,
  });
});

// Preference policy test for the following:
// * Status: default
// * Value: true
add_task(async function () {
  await doPolicyTest({
    prefPolicy: {
      Status: "default",
      Value: true,
    },
    expectedDefault: true,
    expectedUser: undefined,
    expectedLocked: false,
  });
});

// Preference policy test for the following:
// * Status: user
// * Value: false
add_task(async function () {
  await doPolicyTest({
    prefPolicy: {
      Status: "user",
      Value: false,
    },
    expectedDefault: true,
    expectedUser: false,
    expectedLocked: false,
  });
});

// Preference policy test for the following:
// * Status: user
// * Value: true
add_task(async function () {
  await doPolicyTest({
    prefPolicy: {
      Status: "user",
      Value: true,
    },
    expectedDefault: true,
    // Because the pref is sticky, it's true on the user branch even though it's
    // also true on the default branch. Sticky prefs retain their user-branch
    // values even when they're the same as their default-branch values.
    expectedUser: true,
    expectedLocked: false,
  });
});

/**
 * This tests an enterprise preference policy with one of the quick suggest
 * sticky prefs (defined by `POLICY_PREF`). Pref policies should apply to the
 * quick suggest sticky prefs just as they do to non-sticky prefs.
 *
 * @param {object} options
 *   Options object.
 * @param {object} options.prefPolicy
 *   An object `{ Status, Value }` that will be included in the policy.
 * @param {boolean} options.expectedDefault
 *   The expected default-branch pref value after setting the policy.
 * @param {boolean} options.expectedUser
 *   The expected user-branch pref value after setting the policy or undefined
 *   if the pref should not exist on the user branch.
 * @param {boolean} options.expectedLocked
 *   Whether the pref is expected to be locked after setting the policy.
 */
async function doPolicyTest({
  prefPolicy,
  expectedDefault,
  expectedUser,
  expectedLocked,
}) {
  info(
    "Starting pref policy test: " +
      JSON.stringify({
        prefPolicy,
        expectedDefault,
        expectedUser,
        expectedLocked,
      })
  );

  let pref = POLICY_PREF;

  // Check initial state.
  Assert.ok(
    gDefaultBranch.getBoolPref(pref),
    `${pref} is initially true on default branch (assuming en-US)`
  );
  Assert.ok(
    !gUserBranch.prefHasUserValue(pref),
    `${pref} does not have initial user value`
  );

  // Set up the policy.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      Preferences: {
        [`browser.urlbar.${pref}`]: prefPolicy,
      },
    },
  });
  Assert.equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Policy engine is active"
  );

  // Check the default branch.
  Assert.equal(
    gDefaultBranch.getBoolPref(pref),
    expectedDefault,
    `${pref} has expected default-branch value after setting policy`
  );

  // Check the user branch.
  Assert.equal(
    gUserBranch.prefHasUserValue(pref),
    expectedUser !== undefined,
    `${pref} is on user branch as expected after setting policy`
  );
  if (expectedUser !== undefined) {
    Assert.equal(
      gUserBranch.getBoolPref(pref),
      expectedUser,
      `${pref} has expected user-branch value after setting policy`
    );
  }

  // Check the locked state.
  Assert.equal(
    gDefaultBranch.prefIsLocked(pref),
    expectedLocked,
    `${pref} is locked as expected after setting policy`
  );

  // Clean up.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  Assert.equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.INACTIVE,
    "Policy engine is inactive"
  );

  gDefaultBranch.unlockPref(pref);
  gUserBranch.clearUserPref(pref);
  await QuickSuggestTestUtils.setScenario(null);

  Assert.ok(
    !gDefaultBranch.prefIsLocked(pref),
    `${pref} is not locked after cleanup`
  );
  Assert.ok(
    gDefaultBranch.getBoolPref(pref),
    `${pref} is true on default branch after cleanup (assuming en-US)`
  );
  Assert.ok(
    !gUserBranch.prefHasUserValue(pref),
    `${pref} does not have user value after cleanup`
  );
}
