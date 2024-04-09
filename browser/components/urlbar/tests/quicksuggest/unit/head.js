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

let gAddTasksWithRustSetup;

/**
 * Adds two tasks: One with the Rust backend disabled and one with it enabled.
 * The names of the task functions will be the name of the passed-in task
 * function appended with "_rustDisabled" and "_rustEnabled". If the passed-in
 * task doesn't have a name, "anonymousTask" will be used.
 *
 * Call this with the usual `add_task()` arguments. Additionally, an object with
 * the following properties can be specified as any argument:
 *
 * {boolean} skip_if_rust_enabled
 *   If true, a "_rustEnabled" task won't be added. Useful when Rust is enabled
 *   by default but the task doesn't make sense with Rust and you still want to
 *   test some behavior when Rust is disabled.
 *
 * @param {...any} args
 *   The usual `add_task()` arguments.
 */
function add_tasks_with_rust(...args) {
  let skipIfRustEnabled = false;
  let i = args.findIndex(a => a.skip_if_rust_enabled);
  if (i >= 0) {
    skipIfRustEnabled = true;
    args.splice(i, 1);
  }

  let taskFnIndex = args.findIndex(a => typeof a == "function");
  let taskFn = args[taskFnIndex];

  for (let rustEnabled of [false, true]) {
    let newTaskName =
      (taskFn.name || "anonymousTask") +
      (rustEnabled ? "_rustEnabled" : "_rustDisabled");

    if (rustEnabled && skipIfRustEnabled) {
      info(
        "add_tasks_with_rust: Skipping due to skip_if_rust_enabled: " +
          newTaskName
      );
      continue;
    }

    let newTaskFn = async (...taskFnArgs) => {
      info("add_tasks_with_rust: Setting rustEnabled: " + rustEnabled);
      UrlbarPrefs.set("quicksuggest.rustEnabled", rustEnabled);
      info("add_tasks_with_rust: Done setting rustEnabled: " + rustEnabled);

      // The current backend may now start syncing, so wait for it to finish.
      info("add_tasks_with_rust: Forcing sync");
      await QuickSuggestTestUtils.forceSync();
      info("add_tasks_with_rust: Done forcing sync");

      if (gAddTasksWithRustSetup) {
        info("add_tasks_with_rust: Calling setup function");
        await gAddTasksWithRustSetup();
        info("add_tasks_with_rust: Done calling setup function");
      }

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

    Object.defineProperty(newTaskFn, "name", { value: newTaskName });

    let addTaskArgs = [];
    for (let j = 0; j < args.length; j++) {
      addTaskArgs[j] =
        j == taskFnIndex
          ? newTaskFn
          : Cu.cloneInto(args[j], this, { cloneFunctions: true });
    }
    add_task(...addTaskArgs);
  }
}

/**
 * Registers a setup function that `add_tasks_with_rust()` will await before
 * calling each of your original tasks. Call this at most once in your test file
 * (i.e., in `add_setup()`). This is useful when enabling/disabling Rust has
 * side effects related to your particular test that need to be handled or
 * awaited for each of your tasks. On the other hand, if only one or two of your
 * tasks need special setup, do it directly in those tasks instead of using
 * this.
 *
 * @param {Function} setupFn
 *   A function that will be awaited before your original tasks are called.
 */
function registerAddTasksWithRustSetup(setupFn) {
  gAddTasksWithRustSetup = setupFn;
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
  blockId = 2,
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
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
      isManageable: true,
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
  title = "Amp Suggestion",
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
  requestId = undefined,
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
      requestId,
      displayUrl: url.replace(/^https:\/\//, ""),
      isSponsored: true,
      qsSuggestion: keyword,
      sponsoredImpressionUrl: impressionUrl,
      sponsoredClickUrl: clickUrl,
      sponsoredBlockId: blockId,
      sponsoredAdvertiser: advertiser,
      sponsoredIabCategory: iabCategory,
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
      isManageable: true,
      telemetryType: "adm_sponsored",
      descriptionL10n: { id: "urlbar-result-action-sponsored" },
    },
  };

  if (UrlbarPrefs.get("quickSuggestRustEnabled")) {
    result.payload.source = source || "rust";
    result.payload.provider = provider || "Amp";
    if (result.payload.source == "rust") {
      result.payload.iconBlob = iconBlob;
    } else {
      result.payload.icon = icon;
    }
  } else {
    result.payload.source = source || "remote-settings";
    result.payload.provider = provider || "AdmWikipedia";
    result.payload.icon = icon;
  }

  return result;
}

/**
 * Returns an expected MDN result that can be passed to `check_results()`
 * regardless of whether the Rust backend is enabled.
 *
 * @returns {object}
 *   An object that can be passed to `check_results()`.
 */
function makeMdnResult({ url, title, description }) {
  let finalUrl = new URL(url);
  finalUrl.searchParams.set("utm_medium", "firefox-desktop");
  finalUrl.searchParams.set("utm_source", "firefox-suggest");
  finalUrl.searchParams.set(
    "utm_campaign",
    "firefox-mdn-web-docs-suggestion-experiment"
  );
  finalUrl.searchParams.set("utm_content", "treatment");

  let result = {
    isBestMatch: true,
    suggestedIndex: 1,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
    heuristic: false,
    payload: {
      telemetryType: "mdn",
      title,
      url: finalUrl.href,
      originalUrl: url,
      displayUrl: finalUrl.href.replace(/^https:\/\//, ""),
      description,
      icon: "chrome://global/skin/icons/mdn.svg",
      shouldShowUrl: true,
      bottomTextL10n: { id: "firefox-suggest-mdn-bottom-text" },
    },
  };

  if (UrlbarPrefs.get("quickSuggestRustEnabled")) {
    result.payload.source = "rust";
    result.payload.provider = "Mdn";
  } else {
    result.payload.source = "remote-settings";
    result.payload.provider = "MDNSuggestions";
  }

  return result;
}

/**
 * Returns an expected AMO (addons) result that can be passed to
 * `check_results()` regardless of whether the Rust backend is enabled.
 *
 * @returns {object}
 *   An object that can be passed to `check_results()`.
 */
function makeAmoResult({
  source,
  provider,
  title = "Amo Suggestion",
  description = "Amo description",
  url = "http://example.com/amo",
  originalUrl = "http://example.com/amo",
  icon = null,
  setUtmParams = true,
}) {
  if (setUtmParams) {
    url = new URL(url);
    url.searchParams.set("utm_medium", "firefox-desktop");
    url.searchParams.set("utm_source", "firefox-suggest");
    url = url.href;
  }

  let result = {
    isBestMatch: true,
    suggestedIndex: 1,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      source,
      provider,
      title,
      description,
      url,
      originalUrl,
      icon,
      displayUrl: url.replace(/^https:\/\//, ""),
      shouldShowUrl: true,
      bottomTextL10n: { id: "firefox-suggest-addons-recommended" },
      helpUrl: QuickSuggest.HELP_URL,
      telemetryType: "amo",
    },
  };

  if (UrlbarPrefs.get("quickSuggestRustEnabled")) {
    result.payload.source = source || "rust";
    result.payload.provider = provider || "Amo";
  } else {
    result.payload.source = source || "remote-settings";
    result.payload.provider = provider || "AddonSuggestions";
  }

  return result;
}

/**
 * Returns an expected weather result that can be passed to `check_results()`
 * regardless of whether the Rust backend is enabled.
 *
 * @returns {object}
 *   An object that can be passed to `check_results()`.
 */
function makeWeatherResult({
  source,
  provider,
  telemetryType = undefined,
  temperatureUnit = undefined,
} = {}) {
  if (!temperatureUnit) {
    temperatureUnit =
      Services.locale.regionalPrefsLocales[0] == "en-US" ? "f" : "c";
  }

  let result = {
    type: UrlbarUtils.RESULT_TYPE.DYNAMIC,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    suggestedIndex: 1,
    payload: {
      temperatureUnit,
      url: MerinoTestUtils.WEATHER_SUGGESTION.url,
      iconId: "6",
      requestId: MerinoTestUtils.server.response.body.request_id,
      source: "merino",
      provider: "accuweather",
      dynamicType: "weather",
      city: MerinoTestUtils.WEATHER_SUGGESTION.city_name,
      temperature:
        MerinoTestUtils.WEATHER_SUGGESTION.current_conditions.temperature[
          temperatureUnit
        ],
      currentConditions:
        MerinoTestUtils.WEATHER_SUGGESTION.current_conditions.summary,
      forecast: MerinoTestUtils.WEATHER_SUGGESTION.forecast.summary,
      high: MerinoTestUtils.WEATHER_SUGGESTION.forecast.high[temperatureUnit],
      low: MerinoTestUtils.WEATHER_SUGGESTION.forecast.low[temperatureUnit],
      shouldNavigate: true,
    },
  };

  if (UrlbarPrefs.get("quickSuggestRustEnabled")) {
    result.payload.source = source || "rust";
    result.payload.provider = provider || "Weather";
    if (telemetryType !== null) {
      result.payload.telemetryType = telemetryType || "weather";
    }
  } else {
    result.payload.source = source || "merino";
    result.payload.provider = provider || "accuweather";
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
 *   The primary keyword to use during the test.
 * @param {number} options.keywordBaseIndex
 *   The index in `keyword` to base substring checks around. This function will
 *   test substrings starting at the beginning of keyword and ending at the
 *   following indexes: one index before `keywordBaseIndex`,
 *   `keywordBaseIndex`, `keywordBaseIndex` + 1, `keywordBaseIndex` + 2, and
 *   `keywordBaseIndex` + 3.
 */
async function doShowLessFrequentlyTests({
  feature,
  expectedResult,
  showLessFrequentlyCountPref,
  nimbusCapVariable,
  keyword,
  keywordBaseIndex = keyword.indexOf(" "),
}) {
  // Do some sanity checks on the keyword. Any checks that fail are errors in
  // the test.
  if (keywordBaseIndex <= 0) {
    throw new Error(
      "keywordBaseIndex must be > 0, but it's " + keywordBaseIndex
    );
  }
  if (keyword.length < keywordBaseIndex + 3) {
    throw new Error(
      "keyword must have at least two chars after keywordBaseIndex"
    );
  }

  let tests = [
    {
      showLessFrequentlyCount: 0,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex - 1)]: false,
        [keyword.substring(0, keywordBaseIndex)]: true,
        [keyword.substring(0, keywordBaseIndex + 1)]: true,
        [keyword.substring(0, keywordBaseIndex + 2)]: true,
        [keyword.substring(0, keywordBaseIndex + 3)]: true,
      },
    },
    {
      showLessFrequentlyCount: 1,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex)]: false,
      },
    },
    {
      showLessFrequentlyCount: 2,
      canShowLessFrequently: true,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex + 1)]: false,
      },
    },
    {
      showLessFrequentlyCount: 3,
      canShowLessFrequently: false,
      newSearches: {
        [keyword.substring(0, keywordBaseIndex + 2)]: false,
      },
    },
    {
      showLessFrequentlyCount: 3,
      canShowLessFrequently: false,
      newSearches: {},
    },
  ];

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

/**
 * Queries the Rust component directly and checks the returned suggestions. The
 * point is to make sure the Rust backend passes the correct providers to the
 * Rust component depending on the types of enabled suggestions. Assuming the
 * Rust component isn't buggy, it should return suggestions only for the
 * passed-in providers.
 *
 * @param {object} options
 *   Options object
 * @param {string} options.searchString
 *   The search string.
 * @param {Array} options.tests
 *   Array of test objects: `{ prefs, expectedUrls }`
 *
 *   For each object, the given prefs are set, the Rust component is queried
 *   using the given search string, and the URLs of the returned suggestions are
 *   compared to the given expected URLs (order doesn't matter).
 *
 *   {object} prefs
 *     An object mapping pref names (relative to `browser.urlbar`) to values.
 *     These prefs will be set before querying and should be used to enable or
 *     disable particular types of suggestions.
 *   {Array} expectedUrls
 *     An array of the URLs of the suggestions that are expected to be returned.
 *     The order doesn't matter.
 */
async function doRustProvidersTests({ searchString, tests }) {
  UrlbarPrefs.set("quicksuggest.rustEnabled", true);

  for (let { prefs, expectedUrls } of tests) {
    info(
      "Starting Rust providers test: " + JSON.stringify({ prefs, expectedUrls })
    );

    info("Setting prefs and forcing sync");
    for (let [name, value] of Object.entries(prefs)) {
      UrlbarPrefs.set(name, value);
    }
    await QuickSuggestTestUtils.forceSync();

    info("Querying with search string: " + JSON.stringify(searchString));
    let suggestions = await QuickSuggest.rustBackend.query(searchString);
    info("Got suggestions: " + JSON.stringify(suggestions));

    Assert.deepEqual(
      suggestions.map(s => s.url).sort(),
      expectedUrls.sort(),
      "query() should return the expected suggestions (by URL)"
    );

    info("Clearing prefs and forcing sync");
    for (let name of Object.keys(prefs)) {
      UrlbarPrefs.clear(name);
    }
    await QuickSuggestTestUtils.forceSync();
  }

  info("Clearing rustEnabled pref and forcing sync");
  UrlbarPrefs.clear("quicksuggest.rustEnabled");
  await QuickSuggestTestUtils.forceSync();
}

/**
 * Waits for `Weather` to start and finish a new fetch. Typically you call this
 * before you know a new fetch will start, save but don't await the promise, do
 * the thing that triggers the new fetch, and then await the promise to wait for
 * the fetch to finish.
 *
 * If a fetch is currently ongoing, this will first wait for a new fetch to
 * start, which might not be what you want. If you only want to wait for any
 * ongoing fetch to finish, await `QuickSuggest.weather.fetchPromise` instead.
 */
async function waitForNewWeatherFetch() {
  let { fetchPromise: oldFetchPromise } = QuickSuggest.weather;

  // Wait for a new fetch to start.
  let newFetchPromise;
  await TestUtils.waitForCondition(() => {
    let { fetchPromise } = QuickSuggest.weather;
    if (
      (oldFetchPromise && fetchPromise != oldFetchPromise) ||
      (!oldFetchPromise && fetchPromise)
    ) {
      newFetchPromise = fetchPromise;
      return true;
    }
    return false;
  }, "Waiting for a new weather fetch to start");

  Assert.equal(
    QuickSuggest.weather.fetchPromise,
    newFetchPromise,
    "Sanity check: fetchPromise hasn't changed since waitForCondition returned"
  );

  // Wait for the new fetch to finish.
  await newFetchPromise;
}
