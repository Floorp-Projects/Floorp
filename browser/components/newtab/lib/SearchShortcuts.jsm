/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// List of sites we match against Topsites in order to identify sites
// that should be converted to search Topsites
const SEARCH_SHORTCUTS = [
  {keyword: "@amazon", shortURL: "amazon", url: "https://amazon.com", searchIdentifier: /^amazon/},
  {keyword: "@baidu", shortURL: "baidu", url: "https://baidu.com", searchIdentifier: /^baidu/},
  {keyword: "@google", shortURL: "google", url: "https://google.com", searchIdentifier: /^google/},
  {keyword: "@yandex", shortURL: "yandex", url: "https://yandex.com", searchIdentifier: /^yandex/}
];
this.SEARCH_SHORTCUTS = SEARCH_SHORTCUTS;

// These can be added via the editor but will not be added organically
this.CUSTOM_SEARCH_SHORTCUTS = [
  ...SEARCH_SHORTCUTS,
  {keyword: "@bing", shortURL: "bing", url: "https://bing.com", searchIdentifier: /^bing/},
  {keyword: "@duckduckgo", shortURL: "duckduckgo", url: "https://duckduckgo.com", searchIdentifier: /^ddg/},
  {keyword: "@ebay", shortURL: "ebay", url: "https://ebay.com", searchIdentifier: /^ebay/},
  {keyword: "@twitter", shortURL: "twitter", url: "https://twitter.com", searchIdentifier: /^twitter/},
  {keyword: "@wikipedia", shortURL: "wikipedia", url: "https://wikipedia.org", searchIdentifier: /^wikipedia/}
];

// Note: you must add the activity stream branch to the beginning of this if using outside activity stream
this.SEARCH_SHORTCUTS_EXPERIMENT = "improvesearch.topSiteSearchShortcuts";
this.SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF = "improvesearch.topSiteSearchShortcuts.searchEngines";
this.SEARCH_SHORTCUTS_HAVE_PINNED_PREF = "improvesearch.topSiteSearchShortcuts.havePinned";

function getSearchProvider(candidateShortURL) {
  return SEARCH_SHORTCUTS.filter(match => candidateShortURL === match.shortURL)[0] || null;
}
this.getSearchProvider = getSearchProvider;

const EXPORTED_SYMBOLS = ["getSearchProvider", "SEARCH_SHORTCUTS", "CUSTOM_SEARCH_SHORTCUTS", "SEARCH_SHORTCUTS_EXPERIMENT",
  "SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF", "SEARCH_SHORTCUTS_HAVE_PINNED_PREF"];
