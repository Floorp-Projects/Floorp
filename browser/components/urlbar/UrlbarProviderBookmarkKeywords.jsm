/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers bookmarks with keywords.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderBookmarkKeywords"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  KeywordUtils: "resource://gre/modules/KeywordUtils.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Class used to create the provider.
 */
class ProviderBookmarkKeywords extends UrlbarProvider {
  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "BookmarkKeywords";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return (
      (!queryContext.restrictSource ||
        queryContext.restrictSource == UrlbarTokenizer.RESTRICT.BOOKMARK) &&
      !queryContext.searchMode &&
      queryContext.tokens.length
    );
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   */
  async startQuery(queryContext, addCallback) {
    let keyword = queryContext.tokens[0]?.value;

    let searchString = UrlbarUtils.substringAfter(
      queryContext.searchString,
      keyword
    ).trim();
    let { entry, url, postData } = await KeywordUtils.getBindableKeyword(
      keyword,
      searchString
    );
    if (!entry || !url) {
      return;
    }

    let title;
    if (entry.url.host && searchString) {
      // If we have a search string, the result has the title
      // "host: searchString".
      title = UrlbarUtils.strings.formatStringFromName(
        "bookmarkKeywordSearch",
        [
          entry.url.host,
          queryContext.tokens
            .slice(1)
            .map(t => t.value)
            .join(" "),
        ]
      );
    } else {
      title = UrlbarUtils.unEscapeURIForUI(url);
    }

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.KEYWORD,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        title: [title, UrlbarUtils.HIGHLIGHT.TYPED],
        url: [url, UrlbarUtils.HIGHLIGHT.TYPED],
        keyword: [keyword, UrlbarUtils.HIGHLIGHT.TYPED],
        input: queryContext.searchString,
        postData,
        icon: UrlbarUtils.getIconForUrl(entry.url),
      })
    );
    result.heuristic = true;
    addCallback(this, result);
  }
}

var UrlbarProviderBookmarkKeywords = new ProviderBookmarkKeywords();
