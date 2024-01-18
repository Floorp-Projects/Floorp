/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests dynamic Wikipedia quick suggest results.

"use strict";

const MERINO_SUGGESTIONS = [
  {
    title: "title",
    url: "url",
    is_sponsored: false,
    score: 0.23,
    description: "description",
    icon: "icon",
    full_keyword: "full_keyword",
    advertiser: "dynamic-wikipedia",
    block_id: 0,
    impression_url: "impression_url",
    click_url: "click_url",
    provider: "wikipedia",
  },
];

add_setup(async function init() {
  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    merinoSuggestions: MERINO_SUGGESTIONS,
    prefs: [["suggest.quicksuggest.nonsponsored", true]],
  });
});

// When non-sponsored suggestions are disabled, dynamic Wikipedia suggestions
// should be disabled.
add_task(async function nonsponsoredDisabled() {
  // Disable sponsored suggestions. Dynamic Wikipedia suggestions are
  // non-sponsored, so doing this should not prevent them from being enabled.
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  // First make sure the suggestion is added when non-sponsored suggestions are
  // enabled.
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });

  // Now disable them.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
});

add_task(async function mixedCaseQuery() {
  await check_results({
    context: createContext("TeSt", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });
});

function makeExpectedResult() {
  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    suggestedIndex: -1,
    payload: {
      telemetryType: "wikipedia",
      title: "title",
      url: "url",
      displayUrl: "url",
      isSponsored: false,
      icon: "icon",
      qsSuggestion: "full_keyword",
      source: "merino",
      provider: "wikipedia",
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
    },
  };
}
