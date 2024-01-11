/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  SuggestionsMap: "resource:///modules/urlbar/private/SuggestBackendJs.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const NONSPONSORED_IAB_CATEGORIES = new Set(["5 - Education"]);

/**
 * A feature that manages sponsored adM and non-sponsored Wikpedia (sometimes
 * called "expanded Wikipedia") suggestions in remote settings.
 */
export class AdmWikipedia extends BaseFeature {
  constructor() {
    super();
    this.#suggestionsMap = new lazy.SuggestionsMap();
  }

  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") ||
      lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored")
    );
  }

  get enablingPreferences() {
    return [
      "suggest.quicksuggest.nonsponsored",
      "suggest.quicksuggest.sponsored",
    ];
  }

  get merinoProvider() {
    return "adm";
  }

  get rustSuggestionTypes() {
    return ["Amp", "Wikipedia"];
  }

  getSuggestionTelemetryType(suggestion) {
    return suggestion.is_sponsored ? "adm_sponsored" : "adm_nonsponsored";
  }

  isRustSuggestionTypeEnabled(type) {
    switch (type) {
      case "Amp":
        return lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored");
      case "Wikipedia":
        return lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored");
    }
    this.logger.error("Unknown Rust suggestion type: " + type);
    return false;
  }

  enable(enabled) {
    if (enabled) {
      lazy.QuickSuggest.jsBackend.register(this);
    } else {
      lazy.QuickSuggest.jsBackend.unregister(this);
      this.#suggestionsMap.clear();
    }
  }

  async queryRemoteSettings(searchString) {
    let suggestions = this.#suggestionsMap.get(searchString);
    if (!suggestions) {
      return [];
    }

    // Start each icon fetch at the same time and wait for them all to finish.
    let icons = await Promise.all(
      suggestions.map(({ icon }) => this.#fetchIcon(icon))
    );

    return suggestions.map(suggestion => ({
      full_keyword: this.#getFullKeyword(searchString, suggestion.keywords),
      title: suggestion.title,
      url: suggestion.url,
      click_url: suggestion.click_url,
      impression_url: suggestion.impression_url,
      block_id: suggestion.id,
      advertiser: suggestion.advertiser,
      iab_category: suggestion.iab_category,
      is_sponsored: !NONSPONSORED_IAB_CATEGORIES.has(suggestion.iab_category),
      score: suggestion.score,
      position: suggestion.position,
      icon: icons.shift(),
    }));
  }

  async onRemoteSettingsSync(rs) {
    let dataType = lazy.UrlbarPrefs.get("quickSuggestRemoteSettingsDataType");
    this.logger.debug("Loading remote settings with type: " + dataType);

    let [data] = await Promise.all([
      rs.get({ filters: { type: dataType } }),
      rs
        .get({ filters: { type: "icon" } })
        .then(icons =>
          Promise.all(icons.map(i => rs.attachments.downloadToDisk(i)))
        ),
    ]);
    if (!this.isEnabled) {
      return;
    }

    let suggestionsMap = new lazy.SuggestionsMap();

    this.logger.debug(`Got data with ${data.length} records`);
    for (let record of data) {
      let { buffer } = await rs.attachments.download(record);
      if (!this.isEnabled) {
        return;
      }

      let results = JSON.parse(new TextDecoder("utf-8").decode(buffer));
      this.logger.debug(`Adding ${results.length} results`);
      await suggestionsMap.add(results);
      if (!this.isEnabled) {
        return;
      }
    }

    this.#suggestionsMap = suggestionsMap;
  }

  makeResult(queryContext, suggestion, searchString) {
    let originalUrl;
    if (suggestion.source == "rust") {
      // The Rust backend defines `rawUrl` on AMP suggestions, and its value is
      // what we on desktop call the `originalUrl`, i.e., it's a URL that may
      // contain timestamp templates. Rust does not define `rawUrl` for
      // Wikipedia suggestions, but we have historically included `originalUrl`
      // for both AMP and Wikipedia even though Wikipedia URLs never contain
      // timestamp templates. So, when setting `originalUrl`, fall back to `url`
      // for suggestions without `rawUrl`.
      originalUrl = suggestion.rawUrl ?? suggestion.url;

      // The Rust backend uses camelCase instead of snake_case, and it excludes
      // some properties in non-sponsored suggestions that we expect, so convert
      // the Rust suggestion to a suggestion object we expect here on desktop.
      let desktopSuggestion = {
        title: suggestion.title,
        url: suggestion.url,
        is_sponsored: suggestion.is_sponsored,
        full_keyword: suggestion.fullKeyword,
      };
      if (suggestion.is_sponsored) {
        desktopSuggestion.impression_url = suggestion.impressionUrl;
        desktopSuggestion.click_url = suggestion.clickUrl;
        desktopSuggestion.block_id = suggestion.blockId;
        desktopSuggestion.advertiser = suggestion.advertiser;
        desktopSuggestion.iab_category = suggestion.iabCategory;
      } else {
        desktopSuggestion.advertiser = "Wikipedia";
        desktopSuggestion.iab_category = "5 - Education";
      }
      suggestion = desktopSuggestion;
    } else {
      // Replace the suggestion's template substrings, but first save the
      // original URL before its timestamp template is replaced.
      originalUrl = suggestion.url;
      lazy.QuickSuggest.replaceSuggestionTemplates(suggestion);
    }

    let payload = {
      originalUrl,
      url: suggestion.url,
      title: suggestion.title,
      qsSuggestion: [
        suggestion.full_keyword,
        lazy.UrlbarUtils.HIGHLIGHT.SUGGESTED,
      ],
      isSponsored: suggestion.is_sponsored,
      requestId: suggestion.request_id,
      urlTimestampIndex: suggestion.urlTimestampIndex,
      sponsoredImpressionUrl: suggestion.impression_url,
      sponsoredClickUrl: suggestion.click_url,
      sponsoredBlockId: suggestion.block_id,
      sponsoredAdvertiser: suggestion.advertiser,
      sponsoredIabCategory: suggestion.iab_category,
      helpUrl: lazy.QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
    };

    let result = new lazy.UrlbarResult(
      lazy.UrlbarUtils.RESULT_TYPE.URL,
      lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );

    if (suggestion.is_sponsored) {
      if (!lazy.UrlbarPrefs.get("quickSuggestSponsoredPriority")) {
        result.richSuggestionIconSize = 16;
      }

      result.payload.descriptionL10n = {
        id: "urlbar-result-action-sponsored",
      };
      result.isRichSuggestion = true;
    }

    return result;
  }

  /**
   * Gets the "full keyword" (i.e., suggestion) for a query from a list of
   * keywords. The suggestions data doesn't include full keywords, so we make
   * our own based on the result's keyword phrases and a particular query. We
   * use two heuristics:
   *
   * (1) Find the first keyword phrase that has more words than the query. Use
   *     its first `queryWords.length` words as the full keyword. e.g., if the
   *     query is "moz" and `keywords` is ["moz", "mozi", "mozil", "mozill",
   *     "mozilla", "mozilla firefox"], pick "mozilla firefox", pop off the
   *     "firefox" and use "mozilla" as the full keyword.
   * (2) If there isn't any keyword phrase with more words, then pick the
   *     longest phrase. e.g., pick "mozilla" in the previous example (assuming
   *     the "mozilla firefox" phrase isn't there). That might be the query
   *     itself.
   *
   * @param {string} query
   *   The query string.
   * @param {Array} keywords
   *   An array of suggestion keywords.
   * @returns {string}
   *   The full keyword.
   */
  #getFullKeyword(query, keywords) {
    let longerPhrase;
    let trimmedQuery = query.toLocaleLowerCase().trim();
    let queryWords = trimmedQuery.split(" ");

    for (let phrase of keywords) {
      if (phrase.startsWith(query)) {
        let trimmedPhrase = phrase.trim();
        let phraseWords = trimmedPhrase.split(" ");
        // As an exception to (1), if the query ends with a space, then look for
        // phrases with one more word so that the suggestion includes a word
        // following the space.
        let extra = query.endsWith(" ") ? 1 : 0;
        let len = queryWords.length + extra;
        if (len < phraseWords.length) {
          // We found a phrase with more words.
          return phraseWords.slice(0, len).join(" ");
        }
        if (
          query.length < phrase.length &&
          (!longerPhrase || longerPhrase.length < trimmedPhrase.length)
        ) {
          // We found a longer phrase with the same number of words.
          longerPhrase = trimmedPhrase;
        }
      }
    }
    return longerPhrase || trimmedQuery;
  }

  /**
   * Fetch the icon from RemoteSettings attachments.
   *
   * @param {string} path
   *   The icon's remote settings path.
   */
  async #fetchIcon(path) {
    if (!path) {
      return null;
    }

    let { rs } = lazy.QuickSuggest.jsBackend;
    if (!rs) {
      return null;
    }

    let record = (
      await rs.get({
        filters: { id: `icon-${path}` },
      })
    ).pop();
    if (!record) {
      return null;
    }
    return rs.attachments.downloadToDisk(record);
  }

  #suggestionsMap;
}
