/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that provides an autofill result.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderAutofill"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  AboutPagesUtils: "resource://gre/modules/AboutPagesUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE = {
  AUTOFILL_ORIGIN: 1,
  AUTOFILL_URL: 2,
  AUTOFILL_ADAPTIVE: 3,
};

// `WITH` clause for the autofill queries.  autofill_frecency_threshold.value is
// the mean of all moz_origins.frecency values + stddevMultiplier * one standard
// deviation.  This is inlined directly in the SQL (as opposed to being a custom
// Sqlite function for example) in order to be as efficient as possible.
const SQL_AUTOFILL_WITH = `
    WITH
    frecency_stats(count, sum, squares) AS (
      SELECT
        CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = 'origin_frecency_count') AS REAL),
        CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = 'origin_frecency_sum') AS REAL),
        CAST((SELECT IFNULL(value, 0.0) FROM moz_meta WHERE key = 'origin_frecency_sum_of_squares') AS REAL)
    ),
    autofill_frecency_threshold(value) AS (
      SELECT
        CASE count
        WHEN 0 THEN 0.0
        WHEN 1 THEN sum
        ELSE (sum / count) + (:stddevMultiplier * sqrt((squares - ((sum * sum) / count)) / count))
        END
      FROM frecency_stats
    )
  `;

const SQL_AUTOFILL_FRECENCY_THRESHOLD = `host_frecency >= (
    SELECT value FROM autofill_frecency_threshold
  )`;

function originQuery(where) {
  // `frecency`, `bookmarked` and `visited` are partitioned by the fixed host,
  // without `www.`. `host_prefix` instead is partitioned by full host, because
  // we assume a prefix may not work regardless of `www.`.
  let selectVisited = where.includes("visited")
    ? `MAX(EXISTS(
      SELECT 1 FROM moz_places WHERE origin_id = o.id AND visit_count > 0
    )) OVER (PARTITION BY fixup_url(host)) > 0`
    : "0";
  let selectTitle;
  let joinBookmarks;
  if (where.includes("bookmarked")) {
    selectTitle = "ifnull(b.title, h.title)";
    joinBookmarks = "LEFT JOIN moz_bookmarks b ON b.fk = h.id";
  } else {
    selectTitle = "h.title";
    joinBookmarks = "";
  }
  return `/* do not warn (bug no): cannot use an index to sort */
    ${SQL_AUTOFILL_WITH},
    origins(id, prefix, host_prefix, host, fixed, host_frecency, frecency, bookmarked, visited) AS (
      SELECT
      id,
      prefix,
      first_value(prefix) OVER (
        PARTITION BY host ORDER BY frecency DESC, prefix = "https://" DESC, id DESC
      ),
      host,
      fixup_url(host),
      TOTAL(frecency) OVER (PARTITION BY fixup_url(host)),
      frecency,
      MAX(EXISTS(
        SELECT 1 FROM moz_places WHERE origin_id = o.id AND foreign_count > 0
      )) OVER (PARTITION BY fixup_url(host)),
      ${selectVisited}
      FROM moz_origins o
      WHERE (host BETWEEN :searchString AND :searchString || X'FFFF')
         OR (host BETWEEN 'www.' || :searchString AND 'www.' || :searchString || X'FFFF')
    ),
    matched_origin(host_fixed, url) AS (
      SELECT iif(instr(host, :searchString) = 1, host, fixed) || '/',
             ifnull(:prefix, host_prefix) || host || '/'
      FROM origins
      ${where}
      ORDER BY frecency DESC, prefix = "https://" DESC, id DESC
      LIMIT 1
    )
    SELECT :query_type AS query_type,
           :searchString AS search_string,
           matched_origin.host_fixed AS host_fixed,
           matched_origin.url AS url,
           ${selectTitle} AS title
    FROM matched_origin
    LEFT JOIN moz_places h ON h.url_hash = hash(matched_origin.url)
    ${joinBookmarks}
  `;
}

function urlQuery(where1, where2, isBookmarkContained) {
  // We limit the search to places that are either bookmarked or have a frecency
  // over some small, arbitrary threshold (20) in order to avoid scanning as few
  // rows as possible.  Keep in mind that we run this query every time the user
  // types a key when the urlbar value looks like a URL with a path.
  let selectTitle;
  let joinBookmarks;
  if (isBookmarkContained) {
    selectTitle = "ifnull(b.title, matched_url.title)";
    joinBookmarks = "LEFT JOIN moz_bookmarks b ON b.fk = matched_url.id";
  } else {
    selectTitle = "matched_url.title";
    joinBookmarks = "";
  }
  return `/* do not warn (bug no): cannot use an index to sort */
    WITH matched_url(url, title, frecency, bookmarked, visited, stripped_url, is_exact_match, id) AS (
      SELECT url,
             title,
             frecency,
             foreign_count > 0 AS bookmarked,
             visit_count > 0 AS visited,
             strip_prefix_and_userinfo(url) AS stripped_url,
             strip_prefix_and_userinfo(url) = strip_prefix_and_userinfo(:strippedURL) AS is_exact_match,
             id
      FROM moz_places
      WHERE rev_host = :revHost
            ${where1}
      UNION ALL
      SELECT url,
             title,
             frecency,
             foreign_count > 0 AS bookmarked,
             visit_count > 0 AS visited,
             strip_prefix_and_userinfo(url) AS stripped_url,
             strip_prefix_and_userinfo(url) = 'www.' || strip_prefix_and_userinfo(:strippedURL) AS is_exact_match,
             id
      FROM moz_places
      WHERE rev_host = :revHost || 'www.'
            ${where2}
      ORDER BY is_exact_match DESC, frecency DESC, id DESC
      LIMIT 1
    )
    SELECT :query_type AS query_type,
           :searchString AS search_string,
           :strippedURL AS stripped_url,
           matched_url.url AS url,
           ${selectTitle} AS title
    FROM matched_url
    ${joinBookmarks}
  `;
}

// Queries
const QUERY_ORIGIN_HISTORY_BOOKMARK = originQuery(
  `WHERE bookmarked OR ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`
);

const QUERY_ORIGIN_PREFIX_HISTORY_BOOKMARK = originQuery(
  `WHERE prefix BETWEEN :prefix AND :prefix || X'FFFF'
     AND (bookmarked OR ${SQL_AUTOFILL_FRECENCY_THRESHOLD})`
);

const QUERY_ORIGIN_HISTORY = originQuery(
  `WHERE visited AND ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`
);

const QUERY_ORIGIN_PREFIX_HISTORY = originQuery(
  `WHERE prefix BETWEEN :prefix AND :prefix || X'FFFF'
     AND visited AND ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`
);

const QUERY_ORIGIN_BOOKMARK = originQuery(`WHERE bookmarked`);

const QUERY_ORIGIN_PREFIX_BOOKMARK = originQuery(
  `WHERE prefix BETWEEN :prefix AND :prefix || X'FFFF' AND bookmarked`
);

const QUERY_URL_HISTORY_BOOKMARK = urlQuery(
  `AND (bookmarked OR frecency > 20)
     AND stripped_url COLLATE NOCASE
       BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND (bookmarked OR frecency > 20)
     AND stripped_url COLLATE NOCASE
       BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`,
  true
);

const QUERY_URL_PREFIX_HISTORY_BOOKMARK = urlQuery(
  `AND (bookmarked OR frecency > 20)
     AND url COLLATE NOCASE
       BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND (bookmarked OR frecency > 20)
     AND url COLLATE NOCASE
       BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`,
  true
);

const QUERY_URL_HISTORY = urlQuery(
  `AND (visited OR NOT bookmarked)
     AND frecency > 20
     AND stripped_url COLLATE NOCASE
       BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND (visited OR NOT bookmarked)
     AND frecency > 20
     AND stripped_url COLLATE NOCASE
       BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`,
  false
);

const QUERY_URL_PREFIX_HISTORY = urlQuery(
  `AND (visited OR NOT bookmarked)
     AND frecency > 20
     AND url COLLATE NOCASE
       BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND (visited OR NOT bookmarked)
     AND frecency > 20
     AND url COLLATE NOCASE
       BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`,
  false
);

const QUERY_URL_BOOKMARK = urlQuery(
  `AND bookmarked
     AND stripped_url COLLATE NOCASE
       BETWEEN :strippedURL AND :strippedURL || X'FFFF'`,
  `AND bookmarked
     AND stripped_url COLLATE NOCASE
       BETWEEN 'www.' || :strippedURL AND 'www.' || :strippedURL || X'FFFF'`,
  true
);

const QUERY_URL_PREFIX_BOOKMARK = urlQuery(
  `AND bookmarked
     AND url COLLATE NOCASE
       BETWEEN :prefix || :strippedURL AND :prefix || :strippedURL || X'FFFF'`,
  `AND bookmarked
     AND url COLLATE NOCASE
       BETWEEN :prefix || 'www.' || :strippedURL AND :prefix || 'www.' || :strippedURL || X'FFFF'`,
  true
);

/**
 * Class used to create the provider.
 */
class ProviderAutofill extends UrlbarProvider {
  constructor() {
    super();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "Autofill";
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
  async isActive(queryContext) {
    let instance = this.queryInstance;

    // This is usually reset on canceling or completing the query, but since we
    // query in isActive, it may not have been canceled by the previous call.
    // It is an object with values { result: UrlbarResult, instance: Query }.
    // See the documentation for _getAutofillData for more information.
    this._autofillData = null;

    // First of all, check for the autoFill pref.
    if (!UrlbarPrefs.get("autoFill")) {
      return false;
    }

    if (!queryContext.allowAutofill) {
      return false;
    }

    if (queryContext.tokens.length != 1) {
      return false;
    }

    // Trying to autofill an extremely long string would be expensive, and
    // not particularly useful since the filled part falls out of screen anyway.
    if (queryContext.searchString.length > UrlbarUtils.MAX_TEXT_LENGTH) {
      return false;
    }

    // autoFill can only cope with history, bookmarks, and about: entries.
    if (
      !queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY) &&
      !queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)
    ) {
      return false;
    }

    // Autofill doesn't search tags or titles
    if (
      queryContext.tokens.some(
        t =>
          t.type == UrlbarTokenizer.TYPE.RESTRICT_TAG ||
          t.type == UrlbarTokenizer.TYPE.RESTRICT_TITLE
      )
    ) {
      return false;
    }

    [this._strippedPrefix, this._searchString] = UrlbarUtils.stripURLPrefix(
      queryContext.searchString
    );
    this._strippedPrefix = this._strippedPrefix.toLowerCase();

    // Don't try to autofill if the search term includes any whitespace.
    // This may confuse completeDefaultIndex cause the AUTOCOMPLETE_MATCH
    // tokenizer ends up trimming the search string and returning a value
    // that doesn't match it, or is even shorter.
    if (UrlbarTokenizer.REGEXP_SPACES.test(queryContext.searchString)) {
      return false;
    }

    // Fetch autofill result now, rather than in startQuery. We do this so the
    // muxer doesn't have to wait on autofill for every query, since startQuery
    // will be guaranteed to return a result very quickly using this approach.
    // Bug 1651101 is filed to improve this behaviour.
    let result = await this._getAutofillResult(queryContext);
    if (!result || instance != this.queryInstance) {
      return false;
    }
    this._autofillData = { result, instance };
    return true;
  }

  /**
   * Gets the provider's priority.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   */
  getPriority(queryContext) {
    // Priority search results are restricting.
    if (
      this._autofillData &&
      this._autofillData.instance == this.queryInstance &&
      this._autofillData.result.type == UrlbarUtils.RESULT_TYPE.SEARCH
    ) {
      return 1;
    }

    return 0;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    // Check if the query was cancelled while the autofill result was being
    // fetched. We don't expect this to be true since we also check the instance
    // in isActive and clear _autofillData in cancelQuery, but we sanity check it.
    if (
      !this._autofillData ||
      this._autofillData.instance != this.queryInstance
    ) {
      this.logger.error("startQuery invoked with an invalid _autofillData");
      return;
    }

    this._autofillData.result.heuristic = true;
    addCallback(this, this._autofillData.result);
    this._autofillData = null;
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    if (this._autofillData?.instance == this.queryInstance) {
      this._autofillData = null;
    }
  }

  /**
   * Filters hosts by retaining only the ones over the autofill threshold, then
   * sorts them by their frecency, and extracts the one with the highest value.
   * @param {UrlbarQueryContext} queryContext The current queryContext.
   * @param {Array} hosts Array of host names to examine.
   * @returns {Promise} Resolved when the filtering is complete.
   * @resolves {string} The top matching host, or null if not found.
   */
  async getTopHostOverThreshold(queryContext, hosts) {
    let db = await PlacesUtils.promiseLargeCacheDBConnection();
    let conditions = [];
    // Pay attention to the order of params, since they are not named.
    let params = [UrlbarPrefs.get("autoFill.stddevMultiplier"), ...hosts];
    let sources = queryContext.sources;
    if (
      sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY) &&
      sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)
    ) {
      conditions.push(`(bookmarked OR ${SQL_AUTOFILL_FRECENCY_THRESHOLD})`);
    } else if (sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY)) {
      conditions.push(`visited AND ${SQL_AUTOFILL_FRECENCY_THRESHOLD}`);
    } else if (sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)) {
      conditions.push("bookmarked");
    }

    let rows = await db.executeCached(
      `
        ${SQL_AUTOFILL_WITH},
        origins(id, prefix, host_prefix, host, fixed, host_frecency, frecency, bookmarked, visited) AS (
          SELECT
          id,
          prefix,
          first_value(prefix) OVER (
            PARTITION BY host ORDER BY frecency DESC, prefix = "https://" DESC, id DESC
          ),
          host,
          fixup_url(host),
          TOTAL(frecency) OVER (PARTITION BY fixup_url(host)),
          frecency,
          MAX(EXISTS(
            SELECT 1 FROM moz_places WHERE origin_id = o.id AND foreign_count > 0
          )) OVER (PARTITION BY fixup_url(host)),
          MAX(EXISTS(
            SELECT 1 FROM moz_places WHERE origin_id = o.id AND visit_count > 0
          )) OVER (PARTITION BY fixup_url(host))
          FROM moz_origins o
          WHERE o.host IN (${new Array(hosts.length).fill("?").join(",")})
        )
        SELECT host
        FROM origins
        ${conditions.length ? "WHERE " + conditions.join(" AND ") : ""}
        ORDER BY frecency DESC, prefix = "https://" DESC, id DESC
        LIMIT 1
      `,
      params
    );
    if (!rows.length) {
      return null;
    }
    return rows[0].getResultByName("host");
  }

  /**
   * Obtains the query to search for autofill origin results.
   *
   * @param {UrlbarQueryContext} queryContext
   * @returns {array} consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  _getOriginQuery(queryContext) {
    // At this point, searchString is not a URL with a path; it does not
    // contain a slash, except for possibly at the very end.  If there is
    // trailing slash, remove it when searching here to match the rest of the
    // string because it may be an origin.
    let searchStr = this._searchString.endsWith("/")
      ? this._searchString.slice(0, -1)
      : this._searchString;

    let opts = {
      query_type: QUERYTYPE.AUTOFILL_ORIGIN,
      searchString: searchStr.toLowerCase(),
      stddevMultiplier: UrlbarPrefs.get("autoFill.stddevMultiplier"),
    };
    if (this._strippedPrefix) {
      opts.prefix = this._strippedPrefix;
    }

    if (
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY) &&
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)
    ) {
      return [
        this._strippedPrefix
          ? QUERY_ORIGIN_PREFIX_HISTORY_BOOKMARK
          : QUERY_ORIGIN_HISTORY_BOOKMARK,
        opts,
      ];
    }
    if (queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY)) {
      return [
        this._strippedPrefix
          ? QUERY_ORIGIN_PREFIX_HISTORY
          : QUERY_ORIGIN_HISTORY,
        opts,
      ];
    }
    if (queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)) {
      return [
        this._strippedPrefix
          ? QUERY_ORIGIN_PREFIX_BOOKMARK
          : QUERY_ORIGIN_BOOKMARK,
        opts,
      ];
    }
    throw new Error("Either history or bookmark behavior expected");
  }

  /**
   * Obtains the query to search for autoFill url results.
   *
   * @param {UrlbarQueryContext} queryContext
   * @returns {array} consisting of the correctly optimized query to search the
   *         database with and an object containing the params to bound.
   */
  _getUrlQuery(queryContext) {
    // Try to get the host from the search string.  The host is the part of the
    // URL up to either the path slash, port colon, or query "?".  If the search
    // string doesn't look like it begins with a host, then return; it doesn't
    // make sense to do a URL query with it.
    const urlQueryHostRegexp = /^[^/:?]+/;
    let hostMatch = urlQueryHostRegexp.exec(this._searchString);
    if (!hostMatch) {
      return [null, null];
    }

    let host = hostMatch[0].toLowerCase();
    let revHost =
      host
        .split("")
        .reverse()
        .join("") + ".";

    // Build a string that's the URL stripped of its prefix, i.e., the host plus
    // everything after.  Use queryContext.trimmedSearchString instead of
    // this._searchString because this._searchString has had unEscapeURIForUI()
    // called on it.  It's therefore not necessarily the literal URL.
    let strippedURL = queryContext.trimmedSearchString;
    if (this._strippedPrefix) {
      strippedURL = strippedURL.substr(this._strippedPrefix.length);
    }
    strippedURL = host + strippedURL.substr(host.length);

    let opts = {
      query_type: QUERYTYPE.AUTOFILL_URL,
      searchString: this._searchString,
      revHost,
      strippedURL,
    };
    if (this._strippedPrefix) {
      opts.prefix = this._strippedPrefix;
    }

    if (
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY) &&
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)
    ) {
      return [
        this._strippedPrefix
          ? QUERY_URL_PREFIX_HISTORY_BOOKMARK
          : QUERY_URL_HISTORY_BOOKMARK,
        opts,
      ];
    }
    if (queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY)) {
      return [
        this._strippedPrefix ? QUERY_URL_PREFIX_HISTORY : QUERY_URL_HISTORY,
        opts,
      ];
    }
    if (queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)) {
      return [
        this._strippedPrefix ? QUERY_URL_PREFIX_BOOKMARK : QUERY_URL_BOOKMARK,
        opts,
      ];
    }
    throw new Error("Either history or bookmark behavior expected");
  }

  _getAdaptiveHistoryQuery(queryContext) {
    let sourceCondition;
    if (
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY) &&
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)
    ) {
      sourceCondition = "(h.foreign_count > 0 OR h.frecency > 20)";
    } else if (
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.HISTORY)
    ) {
      sourceCondition =
        "((h.visit_count > 0 OR h.foreign_count = 0) AND h.frecency > 20)";
    } else if (
      queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.BOOKMARKS)
    ) {
      sourceCondition = "h.foreign_count > 0";
    } else {
      return [];
    }

    // Change the search target dependent on user's input contains URI scheme.
    let urlCondition;
    if (this._strippedPrefix) {
      urlCondition =
        "(h.url COLLATE NOCASE BETWEEN :searchString AND :searchString || X'FFFF')";
    } else {
      urlCondition = `
        ((strip_prefix_and_userinfo(h.url) COLLATE NOCASE BETWEEN :searchString AND :searchString || X'FFFF') OR
         (strip_prefix_and_userinfo(h.url) COLLATE NOCASE BETWEEN 'www.' || :searchString AND 'www.' || :searchString || X'FFFF'))
      `;
    }

    let selectTitle;
    let joinBookmarks;
    if (UrlbarUtils.RESULT_SOURCE.BOOKMARKS) {
      selectTitle = "ifnull(b.title, matched.title)";
      joinBookmarks = "LEFT JOIN moz_bookmarks b ON b.fk = matched.id";
    } else {
      selectTitle = "matched.title";
      joinBookmarks = "";
    }

    const params = {
      queryType: QUERYTYPE.AUTOFILL_ADAPTIVE,
      searchString: queryContext.searchString.toLowerCase(),
      useCountThreshold: UrlbarPrefs.get(
        "autoFillAdaptiveHistoryUseCountThreshold"
      ),
    };

    const query = `
      WITH matched(input, url, title, stripped_url, is_exact_match, id) AS (
        SELECT
          i.input AS input,
          h.url AS url,
          h.title AS title,
          strip_prefix_and_userinfo(h.url) AS stripped_url,
          strip_prefix_and_userinfo(h.url) = strip_prefix_and_userinfo(:searchString) AS is_exact_match,
          h.id AS id
        FROM moz_places h
        JOIN moz_inputhistory i ON i.place_id = h.id
        WHERE LENGTH(i.input) != 0
          AND :searchString BETWEEN i.input AND i.input || X'FFFF'
          AND ${sourceCondition}
          AND i.use_count >= :useCountThreshold
          AND ${urlCondition}
        ORDER BY is_exact_match DESC, i.use_count DESC, h.frecency DESC, h.id DESC
        LIMIT 1
      )
      SELECT
        :queryType AS query_type,
        :searchString AS search_string,
        input,
        url,
        ${selectTitle} AS title,
        stripped_url
      FROM matched
      ${joinBookmarks}
    `;
    return [query, params];
  }

  /**
   * Processes a matched row in the Places database.
   * @param {object} row
   *   The matched row.
   * @param {UrlbarQueryContext} queryContext
   * @returns {UrlbarResult} a result generated from the matches row.
   */
  _processRow(row, queryContext) {
    let queryType = row.getResultByName("query_type");
    let searchString = row.getResultByName("search_string");
    let title = row.getResultByName("title");
    let autofilledValue, finalCompleteValue, autofilledType;
    let adaptiveHistoryInput;
    switch (queryType) {
      case QUERYTYPE.AUTOFILL_ORIGIN: {
        autofilledValue = row.getResultByName("host_fixed");
        finalCompleteValue = row.getResultByName("url");
        autofilledType = "origin";
        break;
      }
      case QUERYTYPE.AUTOFILL_URL: {
        let url = row.getResultByName("url");
        let strippedURL = row.getResultByName("stripped_url");

        if (!UrlbarUtils.canAutofillURL(url, strippedURL, true)) {
          return null;
        }

        // We autofill urls to-the-next-slash.
        // http://mozilla.org/foo/bar/baz will be autofilled to:
        //  - http://mozilla.org/f[oo/]
        //  - http://mozilla.org/foo/b[ar/]
        //  - http://mozilla.org/foo/bar/b[az]
        // And, toLowerCase() is preferred over toLocaleLowerCase() here
        // because "COLLATE NOCASE" in the SQL only handles ASCII characters.
        let strippedURLIndex = url
          .toLowerCase()
          .indexOf(strippedURL.toLowerCase());
        let strippedPrefix = url.substr(0, strippedURLIndex);
        let nextSlashIndex = url.indexOf(
          "/",
          strippedURLIndex + strippedURL.length - 1
        );
        if (nextSlashIndex == -1) {
          autofilledValue = url.substr(strippedURLIndex);
        } else {
          autofilledValue = url.substring(strippedURLIndex, nextSlashIndex + 1);
        }
        finalCompleteValue = strippedPrefix + autofilledValue;

        if (finalCompleteValue !== url) {
          title = null;
        }

        autofilledType = "url";
        break;
      }
      case QUERYTYPE.AUTOFILL_ADAPTIVE: {
        adaptiveHistoryInput = row.getResultByName("input");
        finalCompleteValue = row.getResultByName("url");

        if (this._strippedPrefix) {
          autofilledValue = finalCompleteValue;
        } else {
          let strippedURL = row.getResultByName("stripped_url");
          autofilledValue = strippedURL.substring(
            strippedURL.toLowerCase().indexOf(adaptiveHistoryInput)
          );
        }

        autofilledType = "adaptive";
        break;
      }
    }

    // `autofilledValue` is the value that will be set in the input, and it
    // should respect the case of the characters the user has typed so far.
    autofilledValue =
      queryContext.searchString +
      autofilledValue.substring(searchString.length);

    // If more than an origin was autofilled and the user typed the full
    // autofilled value, override the final URL by using the exact value the
    // user typed. This allows the user to visit a URL that differs from the
    // autofilled URL only in character case (for example "wikipedia.org/RAID"
    // vs. "wikipedia.org/Raid") by typing the full desired URL.
    if (
      queryType != QUERYTYPE.AUTOFILL_ORIGIN &&
      queryContext.searchString.length == autofilledValue.length
    ) {
      // Use `new URL().href` to lowercase the domain in the final completed
      // URL. This isn't necessary since domains are case insensitive, but it
      // looks nicer because it means the domain will remain lowercased in the
      // input, and it also reflects the fact that Firefox will visit the
      // lowercased name.
      const originalCompleteValue = new URL(finalCompleteValue).href;
      let strippedAutofilledValue = autofilledValue.substring(
        this._strippedPrefix.length
      );
      finalCompleteValue = new URL(
        finalCompleteValue.substring(
          0,
          finalCompleteValue.length - strippedAutofilledValue.length
        ) + strippedAutofilledValue
      ).href;

      // If the character case of except origin part of the original
      // finalCompleteValue differs from finalCompleteValue that includes user's
      // input, we set title null because it expresses different web page.
      if (finalCompleteValue !== originalCompleteValue) {
        title = null;
      }
    }

    const hasTitle = title !== null;
    if (!hasTitle) {
      let [autofilled] = UrlbarUtils.stripPrefixAndTrim(finalCompleteValue, {
        stripHttp: true,
        trimEmptyQuery: true,
        trimSlash: !this._searchString.includes("/"),
      });
      title = [autofilled, UrlbarUtils.HIGHLIGHT.TYPED];
    }

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        title,
        url: [finalCompleteValue, UrlbarUtils.HIGHLIGHT.TYPED],
        icon: UrlbarUtils.getIconForUrl(finalCompleteValue),
      })
    );
    result.autofill = {
      adaptiveHistoryInput,
      value: autofilledValue,
      selectionStart: queryContext.searchString.length,
      selectionEnd: autofilledValue.length,
      type: autofilledType,
      hasTitle,
    };
    return result;
  }

  async _getAutofillResult(queryContext) {
    // We may be autofilling an about: link.
    let result = this._matchAboutPageForAutofill(queryContext);
    if (result) {
      return result;
    }

    // It may also look like a URL we know from the database.
    result = await this._matchKnownUrl(queryContext);
    if (result) {
      return result;
    }

    // Or we may want to fill a search engine domain regardless of the threshold.
    result = await this._matchSearchEngineDomain(queryContext);
    if (result) {
      return result;
    }

    return null;
  }

  _matchAboutPageForAutofill(queryContext) {
    // Check that the typed query is at least one character longer than the
    // about: prefix.
    if (this._strippedPrefix != "about:" || !this._searchString) {
      return null;
    }

    for (const aboutUrl of AboutPagesUtils.visibleAboutUrls) {
      if (aboutUrl.startsWith(`about:${this._searchString.toLowerCase()}`)) {
        let [trimmedUrl] = UrlbarUtils.stripPrefixAndTrim(aboutUrl, {
          stripHttp: true,
          trimEmptyQuery: true,
          trimSlash: !this._searchString.includes("/"),
        });
        let result = new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.HISTORY,
          ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            title: [trimmedUrl, UrlbarUtils.HIGHLIGHT.TYPED],
            url: [aboutUrl, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: UrlbarUtils.getIconForUrl(aboutUrl),
          })
        );
        let autofilledValue =
          queryContext.searchString +
          aboutUrl.substring(queryContext.searchString.length);
        result.autofill = {
          type: "about",
          value: autofilledValue,
          selectionStart: queryContext.searchString.length,
          selectionEnd: autofilledValue.length,
        };
        return result;
      }
    }
    return null;
  }

  async _matchKnownUrl(queryContext) {
    let conn = await PlacesUtils.promiseLargeCacheDBConnection();
    if (!conn) {
      return null;
    }

    // We try to autofill with adaptive history first.
    if (
      UrlbarPrefs.get("autoFillAdaptiveHistoryEnabled") &&
      UrlbarPrefs.get("autoFillAdaptiveHistoryMinCharsThreshold") <=
        queryContext.searchString.length
    ) {
      const [query, params] = this._getAdaptiveHistoryQuery(queryContext);
      if (query) {
        const resultSet = await conn.executeCached(query, params);
        if (resultSet.length) {
          return this._processRow(resultSet[0], queryContext);
        }
      }
    }

    // The adaptive history query is passed queryContext.searchString (the full
    // search string), but the origin and URL queries are passed the prefix
    // (this._strippedPrefix) and the rest of the search string
    // (this._searchString) separately. The user must specify a non-prefix part
    // to trigger origin and URL autofill.
    if (!this._searchString.length) {
      return null;
    }

    // If search string looks like an origin, try to autofill against origins.
    // Otherwise treat it as a possible URL.  When the string has only one slash
    // at the end, we still treat it as an URL.
    let query, params;
    if (
      UrlbarTokenizer.looksLikeOrigin(this._searchString, {
        ignoreKnownDomains: true,
      })
    ) {
      [query, params] = this._getOriginQuery(queryContext);
    } else {
      [query, params] = this._getUrlQuery(queryContext);
    }

    // _getUrlQuery doesn't always return a query.
    if (query) {
      let rows = await conn.executeCached(query, params);
      if (rows.length) {
        return this._processRow(rows[0], queryContext);
      }
    }
    return null;
  }

  async _matchSearchEngineDomain(queryContext) {
    if (
      !UrlbarPrefs.get("autoFill.searchEngines") ||
      !this._searchString.length
    ) {
      return null;
    }

    // enginesForDomainPrefix only matches against engine domains.
    // Remove an eventual trailing slash from the search string (without the
    // prefix) and check if the resulting string is worth matching.
    // Later, we'll verify that the found result matches the original
    // searchString and eventually discard it.
    let searchStr = this._searchString;
    if (searchStr.indexOf("/") == searchStr.length - 1) {
      searchStr = searchStr.slice(0, -1);
    }
    // If the search string looks more like a url than a domain, bail out.
    if (
      !UrlbarTokenizer.looksLikeOrigin(searchStr, { ignoreKnownDomains: true })
    ) {
      return null;
    }

    // Since we are autofilling, we can only pick one matching engine. Use the
    // first.
    let engine = (await UrlbarSearchUtils.enginesForDomainPrefix(searchStr))[0];
    if (!engine) {
      return null;
    }
    let url = engine.searchForm;
    let domain = engine.getResultDomain();
    // Verify that the match we got is acceptable. Autofilling "example/" to
    // "example.com/" would not be good.
    if (
      (this._strippedPrefix && !url.startsWith(this._strippedPrefix)) ||
      !(domain + "/").includes(this._searchString)
    ) {
      return null;
    }

    // The value that's autofilled in the input is the prefix the user typed, if
    // any, plus the portion of the engine domain that the user typed.  Append a
    // trailing slash too, as is usual with autofill.
    let value =
      this._strippedPrefix + domain.substr(domain.indexOf(searchStr)) + "/";

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
        icon: engine.iconURI?.spec,
      })
    );
    let autofilledValue =
      queryContext.searchString +
      value.substring(queryContext.searchString.length);
    result.autofill = {
      value: autofilledValue,
      selectionStart: queryContext.searchString.length,
      selectionEnd: autofilledValue.length,
    };
    return result;
  }
}

var UrlbarProviderAutofill = new ProviderAutofill();
