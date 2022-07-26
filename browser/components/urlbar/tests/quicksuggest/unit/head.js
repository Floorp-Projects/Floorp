/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../unit/head.js */

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

/**
 * Tests quick suggest prefs migrations.
 *
 * @param {object} testOverrides
 *   An object that modifies how migration is performed. It has the following
 *   properties, and all are optional:
 *
 *   {number} migrationVersion
 *     Migration will stop at this version, so for example you can test
 *     migration only up to version 1 even when the current actual version is
 *     larger than 1.
 *   {object} defaultPrefs
 *     An object that maps pref names (relative to `browser.urlbar`) to
 *     default-branch values. These should be the default prefs for the given
 *     `migrationVersion` and will be set as defaults before migration occurs.
 *
 * @param {string} scenario
 *   The scenario to set at the time migration occurs.
 * @param {object} expectedPrefs
 *   The expected prefs after migration: `{ defaultBranch, userBranch }`
 *   Pref names should be relative to `browser.urlbar`.
 * @param {object} [initialUserBranch]
 *   Prefs to set on the user branch before migration ocurs. Use these to
 *   simulate user actions like disabling prefs or opting in or out of the
 *   online modal. Pref names should be relative to `browser.urlbar`.
 */
async function doMigrateTest({
  testOverrides,
  scenario,
  expectedPrefs,
  initialUserBranch = {},
}) {
  info(
    "Testing migration: " +
      JSON.stringify({
        testOverrides,
        initialUserBranch,
        scenario,
        expectedPrefs,
      })
  );

  function setPref(branch, name, value) {
    switch (typeof value) {
      case "boolean":
        branch.setBoolPref(name, value);
        break;
      case "number":
        branch.setIntPref(name, value);
        break;
      case "string":
        branch.setCharPref(name, value);
        break;
      default:
        Assert.ok(
          false,
          `Pref type not handled for setPref: ${name} = ${value}`
        );
        break;
    }
  }

  function getPref(branch, name) {
    let type = typeof UrlbarPrefs.get(name);
    switch (type) {
      case "boolean":
        return branch.getBoolPref(name);
      case "number":
        return branch.getIntPref(name);
      case "string":
        return branch.getCharPref(name);
      default:
        Assert.ok(false, `Pref type not handled for getPref: ${name} ${type}`);
        break;
    }
    return null;
  }

  let defaultBranch = Services.prefs.getDefaultBranch("browser.urlbar.");
  let userBranch = Services.prefs.getBranch("browser.urlbar.");

  // Set initial prefs. `initialDefaultBranch` are firefox.js values, i.e.,
  // defaults immediately after startup and before any scenario update and
  // migration happens.
  UrlbarPrefs._updatingFirefoxSuggestScenario = true;
  UrlbarPrefs.clear("quicksuggest.migrationVersion");
  let initialDefaultBranch = {
    "suggest.quicksuggest.nonsponsored": false,
    "suggest.quicksuggest.sponsored": false,
    "quicksuggest.dataCollection.enabled": false,
  };
  for (let name of Object.keys(initialDefaultBranch)) {
    userBranch.clearUserPref(name);
  }
  for (let [branch, prefs] of [
    [defaultBranch, initialDefaultBranch],
    [userBranch, initialUserBranch],
  ]) {
    for (let [name, value] of Object.entries(prefs)) {
      if (value !== undefined) {
        setPref(branch, name, value);
      }
    }
  }
  UrlbarPrefs._updatingFirefoxSuggestScenario = false;

  // Update the scenario and check prefs twice. The first time the migration
  // should happen, and the second time the migration should not happen and
  // all the prefs should stay the same.
  for (let i = 0; i < 2; i++) {
    info(`Calling updateFirefoxSuggestScenario, i=${i}`);

    // Do the scenario update and set `isStartup` to simulate startup.
    await UrlbarPrefs.updateFirefoxSuggestScenario({
      ...testOverrides,
      scenario,
      isStartup: true,
    });

    // Check expected pref values. Store expected effective values as we go so
    // we can check them afterward. For a given pref, the expected effective
    // value is the user value, or if there's not a user value, the default
    // value.
    let expectedEffectivePrefs = {};
    let {
      defaultBranch: expectedDefaultBranch,
      userBranch: expectedUserBranch,
    } = expectedPrefs;
    expectedDefaultBranch = expectedDefaultBranch || {};
    expectedUserBranch = expectedUserBranch || {};
    for (let [branch, prefs, branchType] of [
      [defaultBranch, expectedDefaultBranch, "default"],
      [userBranch, expectedUserBranch, "user"],
    ]) {
      let entries = Object.entries(prefs);
      if (!entries.length) {
        continue;
      }

      info(
        `Checking expected prefs on ${branchType} branch after updating scenario`
      );
      for (let [name, value] of entries) {
        expectedEffectivePrefs[name] = value;
        if (branch == userBranch) {
          Assert.ok(
            userBranch.prefHasUserValue(name),
            `Pref ${name} is on user branch`
          );
        }
        Assert.equal(
          getPref(branch, name),
          value,
          `Pref ${name} value on ${branchType} branch`
        );
      }
    }

    info(
      `Making sure prefs on the default branch without expected user-branch values are not on the user branch`
    );
    for (let name of Object.keys(initialDefaultBranch)) {
      if (!expectedUserBranch.hasOwnProperty(name)) {
        Assert.ok(
          !userBranch.prefHasUserValue(name),
          `Pref ${name} is not on user branch`
        );
      }
    }

    info(`Checking expected effective prefs`);
    for (let [name, value] of Object.entries(expectedEffectivePrefs)) {
      Assert.equal(
        UrlbarPrefs.get(name),
        value,
        `Pref ${name} effective value`
      );
    }

    let currentVersion =
      testOverrides?.migrationVersion === undefined
        ? UrlbarPrefs.FIREFOX_SUGGEST_MIGRATION_VERSION
        : testOverrides.migrationVersion;
    Assert.equal(
      UrlbarPrefs.get("quicksuggest.migrationVersion"),
      currentVersion,
      "quicksuggest.migrationVersion is correct after migration"
    );
  }

  // Clean up.
  UrlbarPrefs._updatingFirefoxSuggestScenario = true;
  UrlbarPrefs.clear("quicksuggest.migrationVersion");
  let userBranchNames = [
    ...Object.keys(initialUserBranch),
    ...Object.keys(expectedPrefs.userBranch || {}),
  ];
  for (let name of userBranchNames) {
    userBranch.clearUserPref(name);
  }
  UrlbarPrefs._updatingFirefoxSuggestScenario = false;
}
