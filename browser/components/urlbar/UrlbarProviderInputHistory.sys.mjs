/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers input history (aka adaptive
 * history) results. These results map typed search strings to Urlbar results.
 * That way, a user can find a particular result again by typing the same
 * string.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

// Sqlite result row index constants.
const QUERYINDEX = {
  URL: 0,
  TITLE: 1,
  BOOKMARKED: 2,
  BOOKMARKTITLE: 3,
  TAGS: 4,
  SWITCHTAB: 8,
};

// This SQL query fragment provides the following:
//   - whether the entry is bookmarked (QUERYINDEX_BOOKMARKED)
//   - the bookmark title, if it is a bookmark (QUERYINDEX_BOOKMARKTITLE)
//   - the tags associated with a bookmarked entry (QUERYINDEX_TAGS)
const SQL_BOOKMARK_TAGS_FRAGMENT = `EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) AS bookmarked,
   ( SELECT title FROM moz_bookmarks WHERE fk = h.id AND title NOTNULL
     ORDER BY lastModified DESC LIMIT 1
   ) AS btitle,
   ( SELECT GROUP_CONCAT(t.title, ', ')
     FROM moz_bookmarks b
     JOIN moz_bookmarks t ON t.id = +b.parent AND t.parent = :parent
     WHERE b.fk = h.id
   ) AS tags`;

const SQL_ADAPTIVE_QUERY = `/* do not warn (bug 487789) */
   SELECT h.url, h.title, ${SQL_BOOKMARK_TAGS_FRAGMENT}, h.visit_count,
          h.typed, h.id, t.open_count, h.frecency
   FROM (
     SELECT ROUND(MAX(use_count) * (1 + (input = :search_string)), 1) AS rank,
            place_id
     FROM moz_inputhistory
     WHERE input BETWEEN :search_string AND :search_string || X'FFFF'
     GROUP BY place_id
   ) AS i
   JOIN moz_places h ON h.id = i.place_id
   LEFT JOIN moz_openpages_temp t
          ON t.url = h.url
         AND t.userContextId = :userContextId
   WHERE AUTOCOMPLETE_MATCH(NULL, h.url,
                            IFNULL(btitle, h.title), tags,
                            h.visit_count, h.typed, bookmarked,
                            t.open_count,
                            :matchBehavior, :searchBehavior,
                            NULL)
   ORDER BY rank DESC, h.frecency DESC
   LIMIT :maxResults`;

/**
 * Class used to create the provider.
 */
class ProviderInputHistory extends UrlbarProvider {
  /**
   * Unique name for the provider, used by the context to filter on providers.
   */
  get name() {
    return "InputHistory";
  }

  /**
   * The type of the provider, must be one of UrlbarUtils.PROVIDER_TYPE.
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
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
      (lazy.UrlbarPrefs.get("suggest.history") ||
        lazy.UrlbarPrefs.get("suggest.bookmark") ||
        lazy.UrlbarPrefs.get("suggest.openpage")) &&
      !queryContext.searchMode
    );
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;

    let conn = await lazy.PlacesUtils.promiseLargeCacheDBConnection();
    if (instance != this.queryInstance) {
      return;
    }

    let [query, params] = this._getAdaptiveQuery(queryContext);
    let rows = await conn.executeCached(query, params);
    if (instance != this.queryInstance) {
      return;
    }

    for (let row of rows) {
      const url = row.getResultByIndex(QUERYINDEX.URL);
      const openPageCount = row.getResultByIndex(QUERYINDEX.SWITCHTAB) || 0;
      const historyTitle = row.getResultByIndex(QUERYINDEX.TITLE) || "";
      const bookmarked = row.getResultByIndex(QUERYINDEX.BOOKMARKED);
      const bookmarkTitle = bookmarked
        ? row.getResultByIndex(QUERYINDEX.BOOKMARKTITLE)
        : null;
      const tags = row.getResultByIndex(QUERYINDEX.TAGS) || "";

      let resultTitle = historyTitle;
      if (openPageCount > 0 && lazy.UrlbarPrefs.get("suggest.openpage")) {
        if (url == queryContext.currentPage) {
          // Don't suggest switching to the current page.
          continue;
        }
        let result = new lazy.UrlbarResult(
          UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
          UrlbarUtils.RESULT_SOURCE.TABS,
          ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            url: [url, UrlbarUtils.HIGHLIGHT.TYPED],
            title: [resultTitle, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: UrlbarUtils.getIconForUrl(url),
          })
        );
        addCallback(this, result);
        continue;
      }

      let resultSource;
      if (bookmarked && lazy.UrlbarPrefs.get("suggest.bookmark")) {
        resultSource = UrlbarUtils.RESULT_SOURCE.BOOKMARKS;
        resultTitle = bookmarkTitle || historyTitle;
      } else if (lazy.UrlbarPrefs.get("suggest.history")) {
        resultSource = UrlbarUtils.RESULT_SOURCE.HISTORY;
      } else {
        continue;
      }

      let resultTags = tags
        .split(",")
        .map(t => t.trim())
        .filter(tag => {
          let lowerCaseTag = tag.toLocaleLowerCase();
          return queryContext.tokens.some(token =>
            lowerCaseTag.includes(token.lowerCaseValue)
          );
        })
        .sort();

      let result = new lazy.UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        resultSource,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          url: [url, UrlbarUtils.HIGHLIGHT.TYPED],
          title: [resultTitle, UrlbarUtils.HIGHLIGHT.TYPED],
          tags: [resultTags, UrlbarUtils.HIGHLIGHT.TYPED],
          icon: UrlbarUtils.getIconForUrl(url),
        })
      );

      addCallback(this, result);
    }
  }

  /**
   * Obtains the query to search for adaptive results.
   * @param {UrlbarQueryContext} queryContext
   *   The current queryContext.
   * @returns {array} Contains the optimized query with which to search the
   *  database and an object containing the params to bound.
   */
  _getAdaptiveQuery(queryContext) {
    return [
      SQL_ADAPTIVE_QUERY,
      {
        parent: lazy.PlacesUtils.tagsFolderId,
        search_string: queryContext.searchString.toLowerCase(),
        matchBehavior: Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE,
        searchBehavior: lazy.UrlbarPrefs.get("defaultBehavior"),
        userContextId: lazy.UrlbarProviderOpenTabs.getUserContextIdForOpenPagesTable(
          queryContext.userContextId,
          queryContext.isPrivate
        ),
        maxResults: queryContext.maxResults,
      },
    ];
  }
}

export var UrlbarProviderInputHistory = new ProviderInputHistory();
