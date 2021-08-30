/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Merino integration with the quick suggest feature/provider.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

// relative to `browser.urlbar`
const PREF_MERINO_ENABLED = "merino.enabled";
const PREF_REMOTE_SETTINGS_ENABLED = "quicksuggest.remoteSettings.enabled";
const PREF_MERINO_ENDPOINT_URL = "merino.endpointURL";

const REMOTE_SETTINGS_DATA = [
  {
    id: 1,
    url: "http://test.com/q=frabbits",
    title: "frabbits",
    keywords: ["fra", "frab"],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
  },
];

let gMerinoResponse;

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest", true);
  UrlbarPrefs.set("quicksuggest.shouldShowOnboardingDialog", false);

  // Set up the Merino server.
  let path = "/merino";
  let server = makeMerinoServer(path);
  let url = new URL("http://localhost/");
  url.pathname = path;
  url.port = server.identity.primaryPort;
  UrlbarPrefs.set(PREF_MERINO_ENDPOINT_URL, url.toString());

  // Set up the remote settings client with the test data.
  await UrlbarQuickSuggest.init();
  await UrlbarQuickSuggest._processSuggestionsJSON(REMOTE_SETTINGS_DATA);
  UrlbarQuickSuggest.onEnabledUpdate = () => {};
});

// Tests with Merino enabled and remote settings disabled.
add_task(async function merinoOnly() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  setMerinoResponse({
    body: {
      suggestions: [
        {
          full_keyword: "merinoOnly full_keyword",
          title: "merinoOnly title",
          url: "merinoOnly url",
          icon: "merinoOnly icon",
          impression_url: "merinoOnly impression_url",
          click_url: "merinoOnly click_url",
          block_id: 111,
          advertiser: "merinoOnly advertiser",
          is_sponsored: true,
        },
      ],
    },
  });

  let context = createContext("frab", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          qsSuggestion: "merinoOnly full_keyword",
          title: "merinoOnly title",
          url: "merinoOnly url",
          icon: "merinoOnly icon",
          sponsoredImpressionUrl: "merinoOnly impression_url",
          sponsoredClickUrl: "merinoOnly click_url",
          sponsoredBlockId: 111,
          sponsoredAdvertiser: "merinoOnly advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "merinoOnly url",
        },
      },
    ],
  });
});

// Tests with Merino disabled and remote settings enabled.
add_task(async function remoteSettingsOnly() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, false);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  // Make sure the server is prepared to return a response so we can make sure
  // we don't fetch it.
  setMerinoResponse({
    body: {
      suggestions: [
        {
          full_keyword: "remoteSettingsOnly full_keyword",
          title: "remoteSettingsOnly title",
          url: "remoteSettingsOnly url",
          icon: "remoteSettingsOnly icon",
          impression_url: "remoteSettingsOnly impression_url",
          click_url: "remoteSettingsOnly click_url",
          block_id: 111,
          advertiser: "remoteSettingsOnly advertiser",
          is_sponsored: true,
        },
      ],
    },
  });

  let context = createContext("frab", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          qsSuggestion: "frab",
          title: "frabbits",
          url: "http://test.com/q=frabbits",
          icon: null,
          sponsoredImpressionUrl: "http://impression.reporting.test.com/",
          sponsoredClickUrl: "http://click.reporting.test.com/",
          sponsoredBlockId: 1,
          sponsoredAdvertiser: "testadvertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "http://test.com/q=frabbits",
        },
      },
    ],
  });
});

// Tests with both Merino and remote settings enabled. The Merino suggestion
// should be preferred over the remote settings one.
add_task(async function bothEnabled() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  setMerinoResponse({
    body: {
      suggestions: [
        {
          full_keyword: "bothEnabled full_keyword",
          title: "bothEnabled title",
          url: "bothEnabled url",
          icon: "bothEnabled icon",
          impression_url: "bothEnabled impression_url",
          click_url: "bothEnabled click_url",
          block_id: 111,
          advertiser: "bothEnabled advertiser",
          is_sponsored: true,
        },
      ],
    },
  });

  let context = createContext("frab", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          qsSuggestion: "bothEnabled full_keyword",
          title: "bothEnabled title",
          url: "bothEnabled url",
          icon: "bothEnabled icon",
          sponsoredImpressionUrl: "bothEnabled impression_url",
          sponsoredClickUrl: "bothEnabled click_url",
          sponsoredBlockId: 111,
          sponsoredAdvertiser: "bothEnabled advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "bothEnabled url",
        },
      },
    ],
  });
});

// Tests with both Merino and remote settings disabled.
add_task(async function bothDisabled() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, false);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  // Make sure the server is prepared to return a response so we can make sure
  // we don't fetch it.
  setMerinoResponse({
    body: {
      suggestions: [
        {
          full_keyword: "bothDisabled full_keyword",
          title: "bothDisabled title",
          url: "bothDisabled url",
          icon: "bothDisabled icon",
          impression_url: "bothDisabled impression_url",
          click_url: "bothDisabled click_url",
          block_id: 111,
          advertiser: "bothDisabled advertiser",
          is_sponsored: true,
        },
      ],
    },
  });

  let context = createContext("frab", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });
});

// Checks that we use the first Merino suggestion if the response includes
// multiple suggestions.
add_task(async function multipleMerinoSuggestions() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  setMerinoResponse({
    body: {
      suggestions: [
        {
          full_keyword: "multipleMerinoSuggestions 0 full_keyword",
          title: "multipleMerinoSuggestions 0 title",
          url: "multipleMerinoSuggestions 0 url",
          icon: "multipleMerinoSuggestions 0 icon",
          impression_url: "multipleMerinoSuggestions 0 impression_url",
          click_url: "multipleMerinoSuggestions 0 click_url",
          block_id: 0,
          advertiser: "multipleMerinoSuggestions 0 advertiser",
          is_sponsored: true,
        },
        {
          full_keyword: "multipleMerinoSuggestions 1 full_keyword",
          title: "multipleMerinoSuggestions 1 title",
          url: "multipleMerinoSuggestions 1 url",
          icon: "multipleMerinoSuggestions 1 icon",
          impression_url: "multipleMerinoSuggestions 1 impression_url",
          click_url: "multipleMerinoSuggestions 1 click_url",
          block_id: 1,
          advertiser: "multipleMerinoSuggestions 1 advertiser",
          is_sponsored: false,
        },
      ],
    },
  });

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          qsSuggestion: "multipleMerinoSuggestions 0 full_keyword",
          title: "multipleMerinoSuggestions 0 title",
          url: "multipleMerinoSuggestions 0 url",
          icon: "multipleMerinoSuggestions 0 icon",
          sponsoredImpressionUrl: "multipleMerinoSuggestions 0 impression_url",
          sponsoredClickUrl: "multipleMerinoSuggestions 0 click_url",
          sponsoredBlockId: 0,
          sponsoredAdvertiser: "multipleMerinoSuggestions 0 advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "multipleMerinoSuggestions 0 url",
        },
      },
    ],
  });
});

// Checks a response that's valid but also has some unexpected properties.
add_task(async function unexpectedResponseProperties() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  setMerinoResponse({
    body: {
      unexpectedString: "some value",
      unexpectedArray: ["a", "b", "c"],
      unexpectedObject: { foo: "bar" },
      suggestions: [
        {
          full_keyword: "unexpected full_keyword",
          title: "unexpected title",
          url: "unexpected url",
          icon: "unexpected icon",
          impression_url: "unexpected impression_url",
          click_url: "unexpected click_url",
          block_id: 1234,
          advertiser: "unexpected advertiser",
          is_sponsored: true,
        },
      ],
    },
  });

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          qsSuggestion: "unexpected full_keyword",
          title: "unexpected title",
          url: "unexpected url",
          icon: "unexpected icon",
          sponsoredImpressionUrl: "unexpected impression_url",
          sponsoredClickUrl: "unexpected click_url",
          sponsoredBlockId: 1234,
          sponsoredAdvertiser: "unexpected advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "unexpected url",
        },
      },
    ],
  });
});

// Checks some bad/unexpected responses.
add_task(async function badResponses() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  let context;
  let contextArgs = [
    "test",
    { providers: [UrlbarProviderQuickSuggest.name], isPrivate: false },
  ];

  setMerinoResponse({
    body: {},
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    body: { bogus: [] },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    body: { suggestions: {} },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    body: { suggestions: [] },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    body: "",
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    contentType: "text/html",
    body: "bogus",
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
});

function makeMerinoServer(endpointPath) {
  let server = makeTestServer();
  server.registerPathHandler(endpointPath, (req, resp) => {
    resp.setHeader("Content-Type", gMerinoResponse.contentType, false);
    if (typeof gMerinoResponse.body == "string") {
      resp.write(gMerinoResponse.body);
    } else if (gMerinoResponse.body) {
      resp.write(JSON.stringify(gMerinoResponse.body));
    }
  });
  return server;
}

function setMerinoResponse({ body, contentType = "application/json" }) {
  gMerinoResponse = { body, contentType };
}
