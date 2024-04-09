/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
});

// Tests that registering an exposureResults pref and triggering a match causes
// the exposure event to be recorded on the UrlbarResults.
const REMOTE_SETTINGS_RESULTS = [
  QuickSuggestTestUtils.ampRemoteSettings({
    keywords: ["test"],
  }),
  QuickSuggestTestUtils.wikipediaRemoteSettings({
    keywords: ["non_sponsored"],
  }),
];

const EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT = makeAmpResult({
  keyword: "test",
});

const EXPECTED_NON_SPONSORED_REMOTE_SETTINGS_RESULT = makeWikipediaResult({
  keyword: "non_sponsored",
});

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
  UrlbarPrefs.set("exposureResults", suggestResultType("adm_sponsored"));
  UrlbarPrefs.set("showExposureResults", true);

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_sponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, false);
});

add_task(async function testExposureCheckMultiple() {
  UrlbarPrefs.set(
    "exposureResults",
    [
      suggestResultType("adm_sponsored"),
      suggestResultType("adm_nonsponsored"),
    ].join(",")
  );
  UrlbarPrefs.set("showExposureResults", true);

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_sponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, false);

  context = createContext("non_sponsored", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_NON_SPONSORED_REMOTE_SETTINGS_RESULT],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_nonsponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, false);
});

add_task(async function exposureDisplayFiltering() {
  UrlbarPrefs.set("exposureResults", suggestResultType("adm_sponsored"));
  UrlbarPrefs.set("showExposureResults", false);

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  Assert.equal(
    context.results[0].exposureResultType,
    suggestResultType("adm_sponsored")
  );
  Assert.equal(context.results[0].exposureResultHidden, true);
});

function suggestResultType(typeWithoutSource) {
  let source = UrlbarPrefs.get("quickSuggestRustEnabled") ? "rust" : "rs";
  return `${source}_${typeWithoutSource}`;
}

// Copied from quicksuggest/unit/head.js
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

// Copied from quicksuggest/unit/head.js
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
