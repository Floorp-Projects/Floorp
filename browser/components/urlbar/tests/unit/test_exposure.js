/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
});

// Tests that registering an exposureResults pref and triggering a match causes
// the exposure event to be recorded on the UrlbarResults.
const REMOTE_SETTINGS_RESULTS = [
  {
    id: 1,
    url: "http://test.com/q=frabbits",
    title: "frabbits",
    keywords: ["test"],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
  },
  {
    id: 2,
    url: "http://test.com/q=frabbits",
    title: "frabbits",
    keywords: ["non_sponsored"],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "wikipedia",
    iab_category: "5 - Education",
  },
];

const EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    telemetryType: "adm_sponsored",
    qsSuggestion: "test",
    title: "frabbits",
    url: "http://test.com/q=frabbits",
    originalUrl: "http://test.com/q=frabbits",
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/",
    sponsoredClickUrl: "http://click.reporting.test.com/",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "TestAdvertiser",
    isSponsored: true,
    descriptionL10n: { id: "urlbar-result-action-sponsored" },
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: "urlbar-result-menu-learn-more-about-firefox-suggest",
    },
    isBlockable: true,
    blockL10n: {
      id: "urlbar-result-menu-dismiss-firefox-suggest",
    },
    displayUrl: "http://test.com/q=frabbits",
    source: "remote-settings",
    provider: "AdmWikipedia",
  },
};

const EXPECTED_NON_SPONSORED_REMOTE_SETTINGS_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    telemetryType: "adm_nonsponsored",
    qsSuggestion: "non_sponsored",
    title: "frabbits",
    url: "http://test.com/q=frabbits",
    originalUrl: "http://test.com/q=frabbits",
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/",
    sponsoredClickUrl: "http://click.reporting.test.com/",
    sponsoredBlockId: 2,
    sponsoredAdvertiser: "wikipedia",
    sponsoredIabCategory: "5 - Education",
    isSponsored: false,
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: "urlbar-result-menu-learn-more-about-firefox-suggest",
    },
    isBlockable: true,
    blockL10n: {
      id: "urlbar-result-menu-dismiss-firefox-suggest",
    },
    displayUrl: "http://test.com/q=frabbits",
    source: "remote-settings",
    provider: "AdmWikipedia",
  },
};

add_setup(async function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();

  // Set up the remote settings client with the test data.
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: REMOTE_SETTINGS_RESULTS,
      },
    ],
    prefs: [
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.quicksuggest.sponsored", true],
    ],
  });
});

add_task(async function testExposureCheck() {
  UrlbarPrefs.set("exposureResults", "rs_adm_sponsored");
  UrlbarPrefs.set("showExposureResults", true);

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  Assert.equal(context.results[0].exposureResultType, "rs_adm_sponsored");
  Assert.equal(context.results[0].exposureResultHidden, false);
});

add_task(async function testExposureCheckMultiple() {
  UrlbarPrefs.set("exposureResults", "rs_adm_sponsored,rs_adm_nonsponsored");
  UrlbarPrefs.set("showExposureResults", true);

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  Assert.equal(context.results[0].exposureResultType, "rs_adm_sponsored");
  Assert.equal(context.results[0].exposureResultHidden, false);

  context = createContext("non_sponsored", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_NON_SPONSORED_REMOTE_SETTINGS_RESULT],
  });

  Assert.equal(context.results[0].exposureResultType, "rs_adm_nonsponsored");
  Assert.equal(context.results[0].exposureResultHidden, false);
});

add_task(async function exposureDisplayFiltering() {
  UrlbarPrefs.set("exposureResults", "rs_adm_sponsored");
  UrlbarPrefs.set("showExposureResults", false);

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  Assert.equal(context.results[0].exposureResultType, "rs_adm_sponsored");
  Assert.equal(context.results[0].exposureResultHidden, true);
});
