/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../unit/head.js */
/* eslint-disable jsdoc/require-param */

ChromeUtils.defineESModuleGetters(this, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarProviderAutofill: "resource:///modules/UrlbarProviderAutofill.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
});

add_setup(async function setUpQuickSuggestXpcshellTest() {
  // Initializing TelemetryEnvironment in an xpcshell environment requires
  // jumping through a bunch of hoops. Suggest's use of TelemetryEnvironment is
  // tested in browser tests, and there's no other necessary reason to wait for
  // TelemetryEnvironment initialization in xpcshell tests, so just skip it.
  UrlbarPrefs._testSkipTelemetryEnvironmentInit = true;
});

/**
 * Adds two tasks: One with the Rust backend disabled and one with it enabled.
 * The names of the task functions will be the name of the passed-in task
 * function appended with "_rustDisabled" and "_rustEnabled" respectively. Call
 * with the usual `add_task()` arguments.
 */
function add_tasks_with_rust(...args) {
  let taskFnIndex = args.findIndex(a => typeof a == "function");
  let taskFn = args[taskFnIndex];

  for (let rustEnabled of [false, true]) {
    let newTaskFn = async (...taskFnArgs) => {
      info("add_tasks_with_rust: Setting rustEnabled: " + rustEnabled);
      UrlbarPrefs.set("quicksuggest.rustEnabled", rustEnabled);
      info("add_tasks_with_rust: Done setting rustEnabled: " + rustEnabled);

      // The current backend may now start syncing, so wait for it to finish.
      info("add_tasks_with_rust: Forcing sync");
      await QuickSuggestTestUtils.forceSync();
      info("add_tasks_with_rust: Done forcing sync");

      let rv;
      try {
        info(
          "add_tasks_with_rust: Calling original task function: " + taskFn.name
        );
        rv = await taskFn(...taskFnArgs);
      } catch (e) {
        // Clearly report any unusual errors to make them easier to spot and to
        // make the flow of the test clearer. The harness throws NS_ERROR_ABORT
        // when a normal assertion fails, so don't report that.
        if (e.result != Cr.NS_ERROR_ABORT) {
          Assert.ok(
            false,
            "add_tasks_with_rust: The original task function threw an error: " +
              e
          );
        }
        throw e;
      } finally {
        info(
          "add_tasks_with_rust: Done calling original task function: " +
            taskFn.name
        );
        info("add_tasks_with_rust: Clearing rustEnabled");
        UrlbarPrefs.clear("quicksuggest.rustEnabled");
        info("add_tasks_with_rust: Done clearing rustEnabled");

        // The current backend may now start syncing, so wait for it to finish.
        info("add_tasks_with_rust: Forcing sync");
        await QuickSuggestTestUtils.forceSync();
        info("add_tasks_with_rust: Done forcing sync");
      }
      return rv;
    };

    Object.defineProperty(newTaskFn, "name", {
      value: taskFn.name + (rustEnabled ? "_rustEnabled" : "_rustDisabled"),
    });
    let addTaskArgs = [...args];
    addTaskArgs[taskFnIndex] = newTaskFn;
    add_task(...addTaskArgs);
  }
}

/**
 * Returns an expected Wikipedia (non-sponsored) result that can be passed to
 * `check_results()` regardless of whether the Rust backend is enabled.
 *
 * @returns {object}
 *   An object that can be passed to `check_results()`.
 */
function makeWikipediaResult({
  source,
  provider,
  keyword = "wikipedia",
  title = "Wikipedia Suggestion",
  url = "http://example.com/wikipedia",
  originalUrl = "http://example.com/wikipedia",
  icon = null,
  iconBlob = new Blob([new Uint8Array([])]),
  impressionUrl = "http://example.com/wikipedia-impression",
  clickUrl = "http://example.com/wikipedia-click",
  blockId = 1,
  advertiser = "Wikipedia",
  iabCategory = "5 - Education",
  suggestedIndex = -1,
  isSuggestedIndexRelativeToGroup = true,
}) {
  let result = {
    suggestedIndex,
    isSuggestedIndexRelativeToGroup,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      title,
      url,
      originalUrl,
      displayUrl: url.replace(/^https:\/\//, ""),
      isSponsored: false,
      qsSuggestion: keyword,
      sponsoredAdvertiser: "Wikipedia",
      sponsoredIabCategory: "5 - Education",
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
      telemetryType: "adm_nonsponsored",
    },
  };

  if (UrlbarPrefs.get("quickSuggestRustEnabled")) {
    result.payload.source = source || "rust";
    result.payload.provider = provider || "Wikipedia";
    result.payload.iconBlob = iconBlob;
  } else {
    result.payload.source = source || "remote-settings";
    result.payload.provider = provider || "AdmWikipedia";
    result.payload.icon = icon;
    result.payload.sponsoredImpressionUrl = impressionUrl;
    result.payload.sponsoredClickUrl = clickUrl;
    result.payload.sponsoredBlockId = blockId;
    result.payload.sponsoredAdvertiser = advertiser;
    result.payload.sponsoredIabCategory = iabCategory;
  }

  return result;
}

/**
 * Returns an expected AMP (sponsored) result that can be passed to
 * `check_results()` regardless of whether the Rust backend is enabled.
 *
 * @returns {object}
 *   An object that can be passed to `check_results()`.
 */
function makeAmpResult({
  source,
  provider,
  keyword = "amp",
  title = "AMP Suggestion",
  url = "http://example.com/amp",
  originalUrl = "http://example.com/amp",
  icon = null,
  iconBlob = new Blob([new Uint8Array([])]),
  impressionUrl = "http://example.com/amp-impression",
  clickUrl = "http://example.com/amp-click",
  blockId = 1,
  advertiser = "Amp",
  iabCategory = "22 - Shopping",
  suggestedIndex = -1,
  isSuggestedIndexRelativeToGroup = true,
} = {}) {
  let result = {
    suggestedIndex,
    isSuggestedIndexRelativeToGroup,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      title,
      url,
      originalUrl,
      displayUrl: url.replace(/^https:\/\//, ""),
      isSponsored: true,
      qsSuggestion: keyword,
      sponsoredImpressionUrl: impressionUrl,
      sponsoredClickUrl: clickUrl,
      sponsoredBlockId: blockId,
      sponsoredAdvertiser: advertiser,
      sponsoredIabCategory: iabCategory,
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
      telemetryType: "adm_sponsored",
      descriptionL10n: { id: "urlbar-result-action-sponsored" },
    },
  };

  if (UrlbarPrefs.get("quickSuggestRustEnabled")) {
    result.payload.source = source || "rust";
    result.payload.provider = provider || "Amp";
    result.payload.iconBlob = iconBlob;
  } else {
    result.payload.source = source || "remote-settings";
    result.payload.provider = provider || "AdmWikipedia";
    result.payload.icon = icon;
  }

  return result;
}

/**
 * Tests quick suggest prefs migrations.
 *
 * @param {object} options
 *   The options object.
 * @param {object} options.testOverrides
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
 * @param {string} options.scenario
 *   The scenario to set at the time migration occurs.
 * @param {object} options.expectedPrefs
 *   The expected prefs after migration: `{ defaultBranch, userBranch }`
 *   Pref names should be relative to `browser.urlbar`.
 * @param {object} [options.initialUserBranch]
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

/**
 * Does some "show less frequently" tests where the cap is set in remote
 * settings and Nimbus. See `doOneShowLessFrequentlyTest()`. This function
 * assumes the matching behavior implemented by the given `BaseFeature` is based
 * on matching prefixes of the given keyword starting at the first word. It
 * also assumes the `BaseFeature` provides suggestions in remote settings.
 *
 * @param {object} options
 *   Options object.
 * @param {BaseFeature} options.feature
 *   The feature that provides the suggestion matched by the searches.
 * @param {*} options.expectedResult
 *   The expected result that should be matched, for searches that are expected
 *   to match a result. Can also be a function; it's passed the current search
 *   string and it should return the expected result.
 * @param {string} options.showLessFrequentlyCountPref
 *   The name of the pref that stores the "show less frequently" count being
 *   tested.
 * @param {string} options.nimbusCapVariable
 *   The name of the Nimbus variable that stores the "show less frequently" cap
 *   being tested.
 * @param {object} options.keyword
 *   The primary keyword to use during the test. It must contain more than one
 *   word, and it must have at least two chars after the first space.
 */
async function doShowLessFrequentlyTests({
  feature,
  expectedResult,
  showLessFrequentlyCountPref,
  nimbusCapVariable,
  keyword,
}) {
  // Do some sanity checks on the keyword. Any checks that fail are errors in
  // the test.
  let spaceIndex = keyword.indexOf(" ");
  if (spaceIndex < 0) {
    throw new Error("keyword must contain a space");
  }
  if (spaceIndex == 0) {
    throw new Error("keyword must not start with a space");
  }
  if (keyword.length < spaceIndex + 3) {
    throw new Error("keyword must have at least two chars after the space");
  }

  let tests = [
    {
      showLessFrequentlyCount: 0,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, spaceIndex - 1)]: false,
        [keyword.substring(0, spaceIndex)]: true,
        [keyword.substring(0, spaceIndex + 1)]: true,
        [keyword.substring(0, spaceIndex + 2)]: true,
        [keyword.substring(0, spaceIndex + 3)]: true,
      },
    },
    {
      showLessFrequentlyCount: 1,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, spaceIndex)]: false,
      },
    },
    {
      showLessFrequentlyCount: 2,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, spaceIndex + 1)]: false,
      },
    },
    {
      showLessFrequentlyCount: 3,
      canShowLessFrequently: false,
      newSearches: {
        [keyword.substring(0, spaceIndex + 2)]: false,
      },
    },
    {
      showLessFrequentlyCount: 3,
      canShowLessFrequently: false,
      newSearches: {},
    },
  ];

  // The Rust implementation doesn't support the remote settings config.
  if (!UrlbarPrefs.get("quicksuggest.rustEnabled")) {
    info("Testing 'show less frequently' with cap in remote settings");
    await doOneShowLessFrequentlyTest({
      tests,
      feature,
      expectedResult,
      showLessFrequentlyCountPref,
      rs: {
        show_less_frequently_cap: 3,
      },
    });
  }

  // Nimbus should override remote settings.
  info("Testing 'show less frequently' with cap in Nimbus and remote settings");
  await doOneShowLessFrequentlyTest({
    tests,
    feature,
    expectedResult,
    showLessFrequentlyCountPref,
    rs: {
      show_less_frequently_cap: 10,
    },
    nimbus: {
      [nimbusCapVariable]: 3,
    },
  });
}

/**
 * Does a group of searches, increments the "show less frequently" count, and
 * repeats until all groups are done. The cap can be set by remote settings
 * config and/or Nimbus.
 *
 * @param {object} options
 *   Options object.
 * @param {BaseFeature} options.feature
 *   The feature that provides the suggestion matched by the searches.
 * @param {*} options.expectedResult
 *   The expected result that should be matched, for searches that are expected
 *   to match a result. Can also be a function; it's passed the current search
 *   string and it should return the expected result.
 * @param {string} options.showLessFrequentlyCountPref
 *   The name of the pref that stores the "show less frequently" count being
 *   tested.
 * @param {object} options.tests
 *   An array where each item describes a group of new searches to perform and
 *   expected state. Each item should look like this:
 *   `{ showLessFrequentlyCount, canShowLessFrequently, newSearches }`
 *
 *   {number} showLessFrequentlyCount
 *     The expected value of `showLessFrequentlyCount` before the group of
 *     searches is performed.
 *   {boolean} canShowLessFrequently
 *     The expected value of `canShowLessFrequently` before the group of
 *     searches is performed.
 *   {object} newSearches
 *     An object that maps each search string to a boolean that indicates
 *     whether the first remote settings suggestion should be triggered by the
 *     search string. Searches are cumulative: The intended use is to pass a
 *     large initial group of searches in the first search group, and then each
 *     following `newSearches` is a diff against the previous.
 * @param {object} options.rs
 *   The remote settings config to set.
 * @param {object} options.nimbus
 *   The Nimbus variables to set.
 */
async function doOneShowLessFrequentlyTest({
  feature,
  expectedResult,
  showLessFrequentlyCountPref,
  tests,
  rs = {},
  nimbus = {},
}) {
  // Disable Merino so we trigger only remote settings suggestions. The
  // `BaseFeature` is expected to add remote settings suggestions using keywords
  // start starting with the first word in each full keyword, but the mock
  // Merino server will always return whatever suggestion it's told to return
  // regardless of the search string. That means Merino will return a suggestion
  // for a keyword that's smaller than the first full word.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);

  // Set Nimbus variables and RS config.
  let cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature(nimbus);
  await QuickSuggestTestUtils.withConfig({
    config: rs,
    callback: async () => {
      let cumulativeSearches = {};

      for (let {
        showLessFrequentlyCount,
        canShowLessFrequently,
        newSearches,
      } of tests) {
        info(
          "Starting subtest: " +
            JSON.stringify({
              showLessFrequentlyCount,
              canShowLessFrequently,
              newSearches,
            })
        );

        Assert.equal(
          feature.showLessFrequentlyCount,
          showLessFrequentlyCount,
          "showLessFrequentlyCount should be correct initially"
        );
        Assert.equal(
          UrlbarPrefs.get(showLessFrequentlyCountPref),
          showLessFrequentlyCount,
          "Pref should be correct initially"
        );
        Assert.equal(
          feature.canShowLessFrequently,
          canShowLessFrequently,
          "canShowLessFrequently should be correct initially"
        );

        // Merge the current `newSearches` object into the cumulative object.
        cumulativeSearches = {
          ...cumulativeSearches,
          ...newSearches,
        };

        for (let [searchString, isExpected] of Object.entries(
          cumulativeSearches
        )) {
          info("Doing search: " + JSON.stringify({ searchString, isExpected }));

          let results = [];
          if (isExpected) {
            results.push(
              typeof expectedResult == "function"
                ? expectedResult(searchString)
                : expectedResult
            );
          }

          await check_results({
            context: createContext(searchString, {
              providers: [UrlbarProviderQuickSuggest.name],
              isPrivate: false,
            }),
            matches: results,
          });
        }

        feature.incrementShowLessFrequentlyCount();
      }
    },
  });

  await cleanUpNimbus();
  UrlbarPrefs.clear(showLessFrequentlyCountPref);
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
}
