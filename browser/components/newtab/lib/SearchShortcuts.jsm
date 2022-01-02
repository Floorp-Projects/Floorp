/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// List of sites we match against Topsites in order to identify sites
// that should be converted to search Topsites
const SEARCH_SHORTCUTS = [
  { keyword: "@amazon", shortURL: "amazon", url: "https://amazon.com" },
  { keyword: "@\u767E\u5EA6", shortURL: "baidu", url: "https://baidu.com" },
  { keyword: "@google", shortURL: "google", url: "https://google.com" },
  {
    keyword: "@\u044F\u043D\u0434\u0435\u043A\u0441",
    shortURL: "yandex",
    url: "https://yandex.com",
  },
];
this.SEARCH_SHORTCUTS = SEARCH_SHORTCUTS;

// These can be added via the editor but will not be added organically
this.CUSTOM_SEARCH_SHORTCUTS = [
  ...SEARCH_SHORTCUTS,
  { keyword: "@bing", shortURL: "bing", url: "https://bing.com" },
  {
    keyword: "@duckduckgo",
    shortURL: "duckduckgo",
    url: "https://duckduckgo.com",
  },
  { keyword: "@ebay", shortURL: "ebay", url: "https://ebay.com" },
  { keyword: "@twitter", shortURL: "twitter", url: "https://twitter.com" },
  {
    keyword: "@wikipedia",
    shortURL: "wikipedia",
    url: "https://wikipedia.org",
  },
];

// Note: you must add the activity stream branch to the beginning of this if using outside activity stream
this.SEARCH_SHORTCUTS_EXPERIMENT = "improvesearch.topSiteSearchShortcuts";
this.SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF =
  "improvesearch.topSiteSearchShortcuts.searchEngines";
this.SEARCH_SHORTCUTS_HAVE_PINNED_PREF =
  "improvesearch.topSiteSearchShortcuts.havePinned";

function getSearchProvider(candidateShortURL) {
  return (
    SEARCH_SHORTCUTS.filter(match => candidateShortURL === match.shortURL)[0] ||
    null
  );
}
this.getSearchProvider = getSearchProvider;

// Get the search form URL for a given search keyword. This allows us to pick
// different tippytop icons for the different variants. Sush as yandex.com vs. yandex.ru.
// See more details in bug 1643523.
async function getSearchFormURL(keyword) {
  const engine = await Services.search.getEngineByAlias(keyword);
  return engine?.wrappedJSObject._searchForm;
}
this.getSearchFormURL = getSearchFormURL;

// Check topsite against predefined list of valid search engines
// https://searchfox.org/mozilla-central/rev/ca869724246f4230b272ed1c8b9944596e80d920/toolkit/components/search/nsSearchService.js#939
async function checkHasSearchEngine(keyword) {
  return (await Services.search.getAppProvidedEngines()).find(
    e => e.aliases.includes(keyword) && !e.hidden
  );
}
this.checkHasSearchEngine = checkHasSearchEngine;

const EXPORTED_SYMBOLS = [
  "checkHasSearchEngine",
  "getSearchProvider",
  "getSearchFormURL",
  "SEARCH_SHORTCUTS",
  "CUSTOM_SEARCH_SHORTCUTS",
  "SEARCH_SHORTCUTS_EXPERIMENT",
  "SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF",
  "SEARCH_SHORTCUTS_HAVE_PINNED_PREF",
];
