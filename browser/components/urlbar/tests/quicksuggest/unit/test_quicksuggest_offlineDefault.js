/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests `UrlbarPrefs.updateFirefoxSuggestScenario` in isolation under the
// assumption that the offline scenario should be enabled by default for US en.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  Region: "resource://gre/modules/Region.sys.mjs",
});

// All the prefs that `updateFirefoxSuggestScenario` sets along with the
// expected default-branch values when offline is enabled and when it's not
// enabled.
const PREFS = [
  {
    name: "browser.urlbar.quicksuggest.enabled",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedOfflineValue: true,
    expectedOtherValue: false,
  },
  {
    name: "browser.urlbar.quicksuggest.dataCollection.enabled",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedOfflineValue: QuickSuggestTestUtils.DATA_COLLECTION_OFFLINE,
    expectedOtherValue: false,
  },
  {
    name: "browser.urlbar.quicksuggest.shouldShowOnboardingDialog",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedOfflineValue: false,
    expectedOtherValue: true,
  },
  {
    name: "browser.urlbar.suggest.quicksuggest.nonsponsored",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedOfflineValue: true,
    expectedOtherValue: false,
  },
  {
    name: "browser.urlbar.suggest.quicksuggest.sponsored",
    get: "getBoolPref",
    set: "setBoolPref",
    expectedOfflineValue: true,
    expectedOtherValue: false,
  },
];

add_task(async function init() {
  await QuickSuggestTestUtils.initNimbusFeature();
});

add_task(async function test() {
  let tests = [
    { locale: "en-US", home: "US", expectedOfflineDefault: true },
    { locale: "en-US", home: "CA", expectedOfflineDefault: false },
    { locale: "en-CA", home: "US", expectedOfflineDefault: true },
    { locale: "en-CA", home: "CA", expectedOfflineDefault: false },
    { locale: "en-GB", home: "US", expectedOfflineDefault: true },
    { locale: "en-GB", home: "GB", expectedOfflineDefault: false },
    { locale: "de", home: "US", expectedOfflineDefault: false },
    { locale: "de", home: "DE", expectedOfflineDefault: false },
  ];
  for (let { locale, home, expectedOfflineDefault } of tests) {
    await doTest({ locale, home, expectedOfflineDefault });
  }
});

add_task(async function forceBeta() {
  for (let isBeta of [true, false]) {
    UrlbarPrefs._test_isBeta = isBeta;
    await doTest({
      locale: "en-US",
      home: "US",
      expectedOfflineDefault: true,
      prefs: [
        {
          name: "browser.urlbar.quicksuggest.dataCollection.enabled",
          get: "getBoolPref",
          set: "setBoolPref",
          expectedOfflineValue: isBeta,
          expectedOtherValue: false,
        },
      ],
    });
  }
  delete UrlbarPrefs._test_isBeta;
});

/**
 * Sets the app's locale and region, calls
 * `UrlbarPrefs.updateFirefoxSuggestScenario`, and asserts that the pref values
 * are correct.
 *
 * @param {object} options
 *   Options object.
 * @param {string} options.locale
 *   The locale to simulate.
 * @param {string} options.home
 *   The "home" region to simulate.
 * @param {boolean} options.expectedOfflineDefault
 *   The expected value of whether offline should be enabled by default given
 *   the locale and region.
 * @param {Array} options.prefs
 *   All the prefs that `updateFirefoxSuggestScenario` sets along with the
 *   expected default-branch values when offline is enabled and when it's not
 *   enabled. See `PREFS` for an example.
 */
async function doTest({ locale, home, expectedOfflineDefault, prefs = PREFS }) {
  // Setup: Clear any user values and save original default-branch values.
  for (let pref of prefs) {
    Services.prefs.clearUserPref(pref.name);
    pref.originalDefault = Services.prefs
      .getDefaultBranch(pref.name)
      [pref.get]("");
  }

  // Set the region and locale, call the function, check the pref values.
  info(`Checking home region ${home} and locale ${locale}`);
  Region._setHomeRegion(home, false);
  await QuickSuggestTestUtils.withLocales([locale], async () => {
    await UrlbarPrefs.updateFirefoxSuggestScenario();
    for (let { name, get, expectedOfflineValue, expectedOtherValue } of prefs) {
      info(
        "Checking pref: " +
          JSON.stringify({ name, expectedOfflineValue, expectedOtherValue })
      );

      let expectedValue = expectedOfflineDefault
        ? expectedOfflineValue
        : expectedOtherValue;

      // Check the default-branch value.
      Assert.strictEqual(
        Services.prefs.getDefaultBranch(name)[get](""),
        expectedValue,
        `Default pref value for ${name}, locale ${locale}, home ${home}`
      );

      // For good measure, also check the return value of `UrlbarPrefs.get`
      // since we use it everywhere. The value should be the same as the
      // default-branch value.
      Assert.strictEqual(
        UrlbarPrefs.get(name.replace("browser.urlbar.", "")),
        expectedValue,
        `UrlbarPrefs.get() value for ${name}, locale ${locale}, home ${home}`
      );
    }
  });

  // Teardown: Restore original default-branch values for the next task.
  for (let { name, originalDefault, set } of prefs) {
    if (originalDefault === undefined) {
      Services.prefs.deleteBranch(name);
    } else {
      Services.prefs.getDefaultBranch(name)[set]("", originalDefault);
    }
  }
}
