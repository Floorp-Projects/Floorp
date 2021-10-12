/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests QuickSuggest configurations.
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

add_task(async function init() {
  await QuickSuggestTestUtils.ensureQuickSuggestInit();
});

// The default is to wait for no browser restarts to show the onboarding dialog
// on the first restart. This tests that we can override it by configuring the
// `showOnboardingDialogOnNthRestart`
add_task(async function test_override_wait_after_n_restarts() {
  // Set up non-Nimbus prefs related to showing the onboarding.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.showedOnboardingDialog", false],
      ["browser.urlbar.quicksuggest.seenRestarts", 0],
    ],
  });

  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "online",
      // Wait for 1 browser restart
      quickSuggestShowOnboardingDialogAfterNRestarts: 1,
    },
    callback: async () => {
      let dialogPromise = BrowserTestUtils.promiseAlertDialog(
        "accept",
        "chrome://browser/content/urlbar/quicksuggestOnboarding.xhtml",
        { isSubDialog: true }
      ).then(() => info("Saw dialog"));
      let prefPromise = TestUtils.waitForPrefChange(
        "browser.urlbar.quicksuggest.showedOnboardingDialog",
        value => value === true
      ).then(() => info("Saw pref change"));

      // Simulate 2 restarts. this function is only called by BrowserGlue
      // on startup, the first restart would be where MR1 was shown then
      // we will show onboarding the 2nd restart after that.
      for (let i = 0; i < 2; i++) {
        info(`Simulating restart ${i + 1}`);
        await UrlbarQuickSuggest.maybeShowOnboardingDialog();
      }

      info("Waiting for dialog and pref change");
      await Promise.all([dialogPromise, prefPromise]);
    },
  });

  await SpecialPowers.popPrefEnv();
  clearOnboardingPrefs();
});

add_task(async function test_skip_onboarding_dialog() {
  // Set up non-Nimbus prefs related to showing the onboarding.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.showedOnboardingDialog", false],
      ["browser.urlbar.quicksuggest.seenRestarts", 0],
    ],
  });
  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestShouldShowOnboardingDialog: false,
    },
    callback: async () => {
      // Simulate 3 restarts.
      for (let i = 0; i < 3; i++) {
        info(`Simulating restart ${i + 1}`);
        await UrlbarQuickSuggest.maybeShowOnboardingDialog();
      }
      Assert.ok(
        !Services.prefs.getBoolPref(
          "browser.urlbar.quicksuggest.showedOnboardingDialog"
        ),
        "The showed onboarding dialog pref should not be set"
      );
    },
  });
  await SpecialPowers.popPrefEnv();
  clearOnboardingPrefs();
});

add_task(async function test_indexes() {
  await UrlbarTestUtils.withExperiment({
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
  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      merinoEnabled: true,
      merinoEndpointURL: "http://example.com/test_merino_config",
      merinoEndpointParamQuery: "test_merino_config_param",
    },
    callback: () => {
      Assert.equal(UrlbarPrefs.get("merinoEnabled"), true, "merinoEnabled");
      Assert.equal(
        UrlbarPrefs.get("merinoEndpointURL"),
        "http://example.com/test_merino_config",
        "merinoEndpointURL"
      );
      Assert.equal(
        UrlbarPrefs.get("merinoEndpointParamQuery"),
        "test_merino_config_param",
        "merinoEndpointParamQuery"
      );
    },
  });
});

add_task(async function test_scenario_online() {
  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "online",
    },
    callback: () => {
      assertScenarioPrefs({
        urlbarPrefs: {
          // prefs
          "quicksuggest.scenario": "online",
          "quicksuggest.enabled": true,
          "quicksuggest.shouldShowOnboardingDialog": true,
          "suggest.quicksuggest": false,
          "suggest.quicksuggest.sponsored": false,

          // Nimbus variables
          quickSuggestScenario: "online",
          quickSuggestEnabled: true,
          quickSuggestShouldShowOnboardingDialog: true,
        },
        defaults: [
          {
            name: "browser.urlbar.quicksuggest.scenario",
            value: "online",
            getter: "getCharPref",
          },
          {
            name: "browser.urlbar.quicksuggest.enabled",
            value: true,
          },
          {
            name: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
            value: true,
          },
          {
            name: "browser.urlbar.suggest.quicksuggest",
            value: false,
          },
          {
            name: "browser.urlbar.suggest.quicksuggest.sponsored",
            value: false,
          },
        ],
      });
    },
  });
});

add_task(async function test_scenario_offline() {
  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "offline",
    },
    callback: () => {
      assertScenarioPrefs({
        urlbarPrefs: {
          // prefs
          "quicksuggest.scenario": "offline",
          "quicksuggest.enabled": true,
          "quicksuggest.shouldShowOnboardingDialog": false,
          "suggest.quicksuggest": true,
          "suggest.quicksuggest.sponsored": true,

          // Nimbus variables
          quickSuggestScenario: "offline",
          quickSuggestEnabled: true,
          quickSuggestShouldShowOnboardingDialog: false,
        },
        defaults: [
          {
            name: "browser.urlbar.quicksuggest.scenario",
            value: "offline",
            getter: "getCharPref",
          },
          {
            name: "browser.urlbar.quicksuggest.enabled",
            value: true,
          },
          {
            name: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
            value: false,
          },
          {
            name: "browser.urlbar.suggest.quicksuggest",
            value: true,
          },
          {
            name: "browser.urlbar.suggest.quicksuggest.sponsored",
            value: true,
          },
        ],
      });
    },
  });
});

add_task(async function test_scenario_history() {
  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "history",
    },
    callback: () => {
      assertScenarioPrefs({
        urlbarPrefs: {
          // prefs
          "quicksuggest.scenario": "history",
          "quicksuggest.enabled": false,
          "quicksuggest.shouldShowOnboardingDialog": true,
          "suggest.quicksuggest": false,
          "suggest.quicksuggest.sponsored": false,

          // Nimbus variables
          quickSuggestScenario: "history",
          quickSuggestEnabled: false,
          quickSuggestShouldShowOnboardingDialog: true,
        },
        defaults: [
          {
            name: "browser.urlbar.quicksuggest.scenario",
            value: "history",
            getter: "getCharPref",
          },
          {
            name: "browser.urlbar.quicksuggest.enabled",
            value: false,
          },
          {
            name: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
            value: true,
          },
          {
            name: "browser.urlbar.suggest.quicksuggest",
            value: false,
          },
          {
            name: "browser.urlbar.suggest.quicksuggest.sponsored",
            value: false,
          },
        ],
      });
    },
  });
});

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

function clearOnboardingPrefs() {
  UrlbarPrefs.clear("suggest.quicksuggest");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.seenRestarts");
}
