/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  QuickSuggestRemoteSettings:
    "resource:///modules/urlbar/private/QuickSuggestRemoteSettings.sys.mjs",
  SuggestionsMap:
    "resource:///modules/urlbar/private/QuickSuggestRemoteSettings.sys.mjs",
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
      lazy.UrlbarPrefs.get("quickSuggestRemoteSettingsEnabled") &&
      (lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") ||
        lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored"))
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

  getSuggestionTelemetryType(suggestion) {
    return suggestion.is_sponsored ? "adm_sponsored" : "adm_nonsponsored";
  }

  enable(enabled) {
    if (enabled) {
      lazy.QuickSuggestRemoteSettings.register(this);
    } else {
      lazy.QuickSuggestRemoteSettings.unregister(this);
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
    // Replace the suggestion's template substrings, but first save the original
    // URL before its timestamp template is replaced.
    let originalUrl = suggestion.url;
    lazy.QuickSuggest.replaceSuggestionTemplates(suggestion);

    let payload = {
      originalUrl,
      url: suggestion.url,
      icon: suggestion.icon,
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
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
    };

    // Determine if the suggestion itself is a best match.
    let isSuggestionBestMatch = false;
    if (lazy.QuickSuggestRemoteSettings.config.best_match) {
      let { best_match } = lazy.QuickSuggestRemoteSettings.config;
      isSuggestionBestMatch =
        best_match.min_search_string_length <= searchString.length &&
        !best_match.blocked_suggestion_ids.includes(suggestion.block_id);
    }

    // Determine if the urlbar result should be a best match.
    let isResultBestMatch =
      isSuggestionBestMatch &&
      lazy.UrlbarPrefs.get("bestMatchEnabled") &&
      lazy.UrlbarPrefs.get("suggest.bestmatch");
    if (isResultBestMatch) {
      // Show the result as a best match. Best match titles don't include the
      // `full_keyword`, and the user's search string is highlighted.
      payload.title = [suggestion.title, lazy.UrlbarUtils.HIGHLIGHT.TYPED];
    } else {
      // Show the result as a usual quick suggest. Include the `full_keyword`
      // and highlight the parts that aren't in the search string.
      payload.title = suggestion.title;
      payload.qsSuggestion = [
        suggestion.full_keyword,
        lazy.UrlbarUtils.HIGHLIGHT.SUGGESTED,
      ];
    }

    // Set `is_top_pick` on the suggestion to tell the provider to set
    // best-match related properties on the result.
    suggestion.is_top_pick = isResultBestMatch;

    payload.isBlockable = lazy.UrlbarPrefs.get(
      isResultBestMatch
        ? "bestMatchBlockingEnabled"
        : "quickSuggestBlockingEnabled"
    );

    let result = new lazy.UrlbarResult(
      lazy.UrlbarUtils.RESULT_TYPE.URL,
      lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );

    if (suggestion.is_sponsored) {
      result.isRichSuggestion = true;
      result.richSuggestionIconSize = 16;
      result.payload.descriptionL10n = { id: "urlbar-result-action-sponsored" };
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

    let { rs } = lazy.QuickSuggestRemoteSettings;
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
