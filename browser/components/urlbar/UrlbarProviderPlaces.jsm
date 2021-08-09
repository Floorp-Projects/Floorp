/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint complexity: ["error", 53] */

"use strict";

/**
 * This module exports a provider that providers results from the Places
 * database, including history, bookmarks, and open tabs.
 */
var EXPORTED_SYMBOLS = ["UrlbarProviderPlaces"];

// Constants

// AutoComplete query type constants.
// Describes the various types of queries that we can process rows for.
const QUERYTYPE_FILTERED = 0;

// The default frecency value used when inserting matches with unknown frecency.
const FRECENCY_DEFAULT = 1000;

// The result is notified on a delay, to avoid rebuilding the panel at every match.
const NOTIFYRESULT_DELAY_MS = 16;

// Sqlite result row index constants.
const QUERYINDEX_QUERYTYPE = 0;
const QUERYINDEX_URL = 1;
const QUERYINDEX_TITLE = 2;
const QUERYINDEX_BOOKMARKED = 3;
const QUERYINDEX_BOOKMARKTITLE = 4;
const QUERYINDEX_TAGS = 5;
//    QUERYINDEX_VISITCOUNT    = 6;
//    QUERYINDEX_TYPED         = 7;
const QUERYINDEX_PLACEID = 8;
const QUERYINDEX_SWITCHTAB = 9;
const QUERYINDEX_FRECENCY = 10;

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

// TODO bug 412736: in case of a frecency tie, we might break it with h.typed
// and h.visit_count.  That is slower though, so not doing it yet...
// NB: as a slight performance optimization, we only evaluate the "bookmarked"
// condition once, and avoid evaluating "btitle" and "tags" when it is false.
function defaultQuery(conditions = "") {
  let query = `SELECT :query_type, h.url, h.title, ${SQL_BOOKMARK_TAGS_FRAGMENT},
            h.visit_count, h.typed, h.id, t.open_count, h.frecency
     FROM moz_places h
     LEFT JOIN moz_openpages_temp t
            ON t.url = h.url
           AND t.userContextId = :userContextId
     WHERE h.frecency <> 0
       AND CASE WHEN bookmarked
         THEN
           AUTOCOMPLETE_MATCH(:searchString, h.url,
                              IFNULL(btitle, h.title), tags,
                              h.visit_count, h.typed,
                              1, t.open_count,
                              :matchBehavior, :searchBehavior)
         ELSE
           AUTOCOMPLETE_MATCH(:searchString, h.url,
                              h.title, '',
                              h.visit_count, h.typed,
                              0, t.open_count,
                              :matchBehavior, :searchBehavior)
         END
       ${conditions ? "AND" : ""} ${conditions}
     ORDER BY h.frecency DESC, h.id DESC
     LIMIT :maxResults`;
  return query;
}

const SQL_SWITCHTAB_QUERY = `SELECT :query_type, t.url, t.url, NULL, NULL, NULL, NULL, NULL, NULL,
          t.open_count, NULL
   FROM moz_openpages_temp t
   LEFT JOIN moz_places h ON h.url_hash = hash(t.url) AND h.url = t.url
   WHERE h.id IS NULL
     AND t.userContextId = :userContextId
     AND AUTOCOMPLETE_MATCH(:searchString, t.url, t.url, NULL,
                            NULL, NULL, NULL, t.open_count,
                            :matchBehavior, :searchBehavior)
   ORDER BY t.ROWID DESC
   LIMIT :maxResults`;

// Getters

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  KeywordUtils: "resource://gre/modules/KeywordUtils.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Sqlite: "resource://gre/modules/Sqlite.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

function setTimeout(callback, ms) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(callback, ms, timer.TYPE_ONE_SHOT);
  return timer;
}

// Maps restriction character types to textual behaviors.
XPCOMUtils.defineLazyGetter(this, "typeToBehaviorMap", () => {
  return new Map([
    [UrlbarTokenizer.TYPE.RESTRICT_HISTORY, "history"],
    [UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK, "bookmark"],
    [UrlbarTokenizer.TYPE.RESTRICT_TAG, "tag"],
    [UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE, "openpage"],
    [UrlbarTokenizer.TYPE.RESTRICT_SEARCH, "search"],
    [UrlbarTokenizer.TYPE.RESTRICT_TITLE, "title"],
    [UrlbarTokenizer.TYPE.RESTRICT_URL, "url"],
  ]);
});

XPCOMUtils.defineLazyGetter(this, "sourceToBehaviorMap", () => {
  return new Map([
    [UrlbarUtils.RESULT_SOURCE.HISTORY, "history"],
    [UrlbarUtils.RESULT_SOURCE.BOOKMARKS, "bookmark"],
    [UrlbarUtils.RESULT_SOURCE.TABS, "openpage"],
    [UrlbarUtils.RESULT_SOURCE.SEARCH, "search"],
  ]);
});

// Helper functions

/**
 * Returns the key to be used for a match in a map for the purposes of removing
 * duplicate entries - any 2 matches that should be considered the same should
 * return the same key.  The type of the returned key depends on the type of the
 * match, so don't assume you can compare keys using ==.  Instead, use
 * ObjectUtils.deepEqual().
 *
 * @param   {object} match
 *          The match object.
 * @returns {value} Some opaque key object.  Use ObjectUtils.deepEqual() to
 *          compare keys.
 */
function makeKeyForMatch(match) {
  let key, prefix;
  let action = PlacesUtils.parseActionUrl(match.value);
  if (!action) {
    [key, prefix] = UrlbarUtils.stripPrefixAndTrim(match.value, {
      stripHttp: true,
      stripHttps: true,
      stripWww: true,
      trimSlash: true,
      trimEmptyQuery: true,
      trimEmptyHash: true,
    });
    return [key, prefix, null];
  }

  switch (action.type) {
    case "searchengine":
      // We want to exclude search suggestion matches that simply echo back the
      // query string in the heuristic result.  For example, if the user types
      // "@engine test", we want to exclude a "test" suggestion match.
      key = [
        action.type,
        action.params.engineName,
        (
          action.params.searchSuggestion || action.params.searchQuery
        ).toLocaleLowerCase(),
      ];
      break;
    default:
      [key, prefix] = UrlbarUtils.stripPrefixAndTrim(
        action.params.url || match.value,
        {
          stripHttp: true,
          stripHttps: true,
          stripWww: true,
          trimEmptyQuery: true,
          trimSlash: true,
        }
      );
      break;
  }

  return [key, prefix, action];
}

/**
 * Makes a moz-action url for the given action and set of parameters.
 *
 * @param   {string} type
 *          The action type.
 * @param   {object} params
 *          A JS object of action params.
 * @returns {string} A moz-action url as a string.
 */
function makeActionUrl(type, params) {
  let encodedParams = {};
  for (let key in params) {
    // Strip null or undefined.
    // Regardless, don't encode them or they would be converted to a string.
    if (params[key] === null || params[key] === undefined) {
      continue;
    }
    encodedParams[key] = encodeURIComponent(params[key]);
  }
  return `moz-action:${type},${JSON.stringify(encodedParams)}`;
}

/**
 * Converts an array of legacy match objects into UrlbarResults.
 * Note that at every call we get the full set of results, included the
 * previously returned ones, and new results may be inserted in the middle.
 * This means we could sort these wrongly, the muxer should take care of it.
 *
 * @param {UrlbarQueryContext} context the query context.
 * @param {array} matches The match objects.
 * @param {set} urls a Set containing all the found urls, used to discard
 *        already added results.
 * @returns {array} converted results
 */
function convertLegacyMatches(context, matches, urls) {
  let results = [];
  for (let match of matches) {
    // First, let's check if we already added this result.
    // `matches` always contains all of the results, includes ones
    // we may have added already. This means we'll end up adding things in the
    // wrong order here, but that's a task for the UrlbarMuxer.
    let url = match.finalCompleteValue || match.value;
    if (urls.has(url)) {
      continue;
    }
    urls.add(url);
    let result = makeUrlbarResult(context.tokens, {
      url,
      // `match.icon` is an empty string if there is no icon. Use undefined
      // instead so that tests can be simplified by not including `icon: ""` in
      // all their payloads.
      icon: match.icon || undefined,
      style: match.style,
      comment: match.comment,
      firstToken: context.tokens[0],
    });
    // Should not happen, but better safe than sorry.
    if (!result) {
      continue;
    }

    results.push(result);
  }
  return results;
}

/**
 * Creates a new UrlbarResult from the provided data.
 * @param {array} tokens the search tokens.
 * @param {object} info includes properties from the legacy result.
 * @returns {object} an UrlbarResult
 */
function makeUrlbarResult(tokens, info) {
  let action = PlacesUtils.parseActionUrl(info.url);
  if (action) {
    switch (action.type) {
      case "searchengine": {
        if (action.params.isSearchHistory) {
          // Return a form history result.
          return new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.SEARCH,
            UrlbarUtils.RESULT_SOURCE.HISTORY,
            ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
              engine: action.params.engineName,
              suggestion: [
                action.params.searchSuggestion,
                UrlbarUtils.HIGHLIGHT.SUGGESTED,
              ],
              lowerCaseSuggestion: action.params.searchSuggestion.toLocaleLowerCase(),
            })
          );
        }

        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.SEARCH,
          UrlbarUtils.RESULT_SOURCE.SEARCH,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            engine: [action.params.engineName, UrlbarUtils.HIGHLIGHT.TYPED],
            suggestion: [
              action.params.searchSuggestion,
              UrlbarUtils.HIGHLIGHT.SUGGESTED,
            ],
            lowerCaseSuggestion: action.params.searchSuggestion?.toLocaleLowerCase(),
            keyword: action.params.alias,
            query: [
              action.params.searchQuery.trim(),
              UrlbarUtils.HIGHLIGHT.NONE,
            ],
            icon: info.icon,
          })
        );
      }
      case "switchtab":
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
          UrlbarUtils.RESULT_SOURCE.TABS,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            url: [action.params.url, UrlbarUtils.HIGHLIGHT.TYPED],
            title: [info.comment, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: info.icon,
          })
        );
      case "visiturl":
        return new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
            title: [info.comment, UrlbarUtils.HIGHLIGHT.TYPED],
            url: [action.params.url, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: info.icon,
          })
        );
      default:
        Cu.reportError(`Unexpected action type: ${action.type}`);
        return null;
    }
  }

  // This is a normal url/title tuple.
  let source;
  let tags = [];
  let comment = info.comment;

  // The legacy autocomplete result may return "bookmark", "bookmark-tag" or
  // "tag". In the last case it should not be considered a bookmark, but an
  // history item with tags. We don't show tags for non bookmarked items though.
  if (info.style.includes("bookmark")) {
    source = UrlbarUtils.RESULT_SOURCE.BOOKMARKS;
  } else {
    source = UrlbarUtils.RESULT_SOURCE.HISTORY;
  }

  // If the style indicates that the result is tagged, then the tags are
  // included in the title, and we must extract them.
  if (info.style.includes("tag")) {
    [comment, tags] = info.comment.split(UrlbarUtils.TITLE_TAGS_SEPARATOR);

    // However, as mentioned above, we don't want to show tags for non-
    // bookmarked items, so we include tags in the final result only if it's
    // bookmarked, and we drop the tags otherwise.
    if (source != UrlbarUtils.RESULT_SOURCE.BOOKMARKS) {
      tags = "";
    }

    // Tags are separated by a comma and in a random order.
    // We should also just include tags that match the searchString.
    tags = tags
      .split(",")
      .map(t => t.trim())
      .filter(tag => {
        let lowerCaseTag = tag.toLocaleLowerCase();
        return tokens.some(token =>
          lowerCaseTag.includes(token.lowerCaseValue)
        );
      })
      .sort();
  }

  return new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    source,
    ...UrlbarResult.payloadAndSimpleHighlights(tokens, {
      url: [info.url, UrlbarUtils.HIGHLIGHT.TYPED],
      icon: info.icon,
      title: [comment, UrlbarUtils.HIGHLIGHT.TYPED],
      tags: [tags, UrlbarUtils.HIGHLIGHT.TYPED],
    })
  );
}

const MATCH_TYPE = {
  HEURISTIC: "heuristic",
  GENERAL: "general",
  SUGGESTION: "suggestion",
  EXTENSION: "extension",
};

/**
 * Manages a single instance of a Places search.
 *
 * @param {UrlbarQueryContext} queryContext
 * @param {function} listener Called as: `listener(matches, searchOngoing)`
 * @param {PlacesProvider} provider
 */
function Search(queryContext, listener, provider) {
  // We want to store the original string for case sensitive searches.
  this._originalSearchString = queryContext.searchString;
  this._trimmedOriginalSearchString = queryContext.trimmedSearchString;
  let unescapedSearchString = Services.textToSubURI.unEscapeURIForUI(
    this._trimmedOriginalSearchString
  );
  let [prefix, suffix] = UrlbarUtils.stripURLPrefix(unescapedSearchString);
  this._searchString = suffix;
  this._strippedPrefix = prefix.toLowerCase();

  this._matchBehavior = Ci.mozIPlacesAutoComplete.MATCH_BOUNDARY;
  // Set the default behavior for this search.
  this._behavior = this._searchString
    ? UrlbarPrefs.get("defaultBehavior")
    : this._emptySearchDefaultBehavior;

  this._inPrivateWindow = queryContext.isPrivate;
  this._prohibitAutoFill = !queryContext.allowAutofill;
  this._maxResults = queryContext.maxResults;
  this._userContextId = queryContext.userContextId;
  this._currentPage = queryContext.currentPage;
  this._searchModeEngine = queryContext.searchMode?.engineName;
  this._searchMode = queryContext.searchMode;
  if (this._searchModeEngine) {
    // Filter Places results on host.
    let engine = Services.search.getEngineByName(this._searchModeEngine);
    this._filterOnHost = engine.getResultDomain();
  }

  this._userContextId = UrlbarProviderOpenTabs.getUserContextIdForOpenPagesTable(
    this._userContextId,
    this._inPrivateWindow
  );

  // Use the original string here, not the stripped one, so the tokenizer can
  // properly recognize token types.
  let { tokens } = UrlbarTokenizer.tokenize({
    searchString: unescapedSearchString,
    trimmedSearchString: unescapedSearchString.trim(),
  });

  // This allows to handle leading or trailing restriction characters specially.
  this._leadingRestrictionToken = null;
  if (tokens.length) {
    if (
      UrlbarTokenizer.isRestrictionToken(tokens[0]) &&
      (tokens.length > 1 ||
        tokens[0].type == UrlbarTokenizer.TYPE.RESTRICT_SEARCH)
    ) {
      this._leadingRestrictionToken = tokens[0].value;
    }

    // Check if the first token has a strippable prefix and remove it, but don't
    // create an empty token.
    if (prefix && tokens[0].value.length > prefix.length) {
      tokens[0].value = tokens[0].value.substring(prefix.length);
    }
  }

  // Eventually filter restriction tokens. In general it's a good idea, but if
  // the consumer requested search mode, we should use the full string to avoid
  // ignoring valid tokens.
  this._searchTokens =
    !queryContext || queryContext.restrictToken
      ? this.filterTokens(tokens)
      : tokens;

  // The behavior can be set through:
  // 1. a specific restrictSource in the QueryContext
  // 2. typed restriction tokens
  if (
    queryContext &&
    queryContext.restrictSource &&
    sourceToBehaviorMap.has(queryContext.restrictSource)
  ) {
    this._behavior = 0;
    this.setBehavior("restrict");
    let behavior = sourceToBehaviorMap.get(queryContext.restrictSource);
    this.setBehavior(behavior);

    // When we are in restrict mode, all the tokens are valid for searching, so
    // there is no _heuristicToken.
    this._heuristicToken = null;
  } else {
    // The heuristic token is the first filtered search token, but only when it's
    // actually the first thing in the search string.  If a prefix or restriction
    // character occurs first, then the heurstic token is null.  We use the
    // heuristic token to help determine the heuristic result.
    let firstToken = !!this._searchTokens.length && this._searchTokens[0].value;
    this._heuristicToken =
      firstToken && this._trimmedOriginalSearchString.startsWith(firstToken)
        ? firstToken
        : null;
  }

  // Set the right JavaScript behavior based on our preference.  Note that the
  // preference is whether or not we should filter JavaScript, and the
  // behavior is if we should search it or not.
  if (!UrlbarPrefs.get("filter.javascript")) {
    this.setBehavior("javascript");
  }

  this._listener = listener;
  this._provider = provider;
  this._matches = [];

  // These are used to avoid adding duplicate entries to the results.
  this._usedURLs = [];
  this._usedPlaceIds = new Set();

  // Counters for the number of results per MATCH_TYPE.
  this._counts = Object.values(MATCH_TYPE).reduce((o, p) => {
    o[p] = 0;
    return o;
  }, {});
}

Search.prototype = {
  /**
   * Enables the desired AutoComplete behavior.
   *
   * @param {string} type
   *        The behavior type to set.
   */
  setBehavior(type) {
    type = type.toUpperCase();
    this._behavior |= Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type];
  },

  /**
   * Determines if the specified AutoComplete behavior is set.
   *
   * @param {string} type
   *        The behavior type to test for.
   * @returns {boolean} true if the behavior is set, false otherwise.
   */
  hasBehavior(type) {
    let behavior = Ci.mozIPlacesAutoComplete["BEHAVIOR_" + type.toUpperCase()];
    return this._behavior & behavior;
  },

  /**
   * Given an array of tokens, this function determines which query should be
   * ran.  It also removes any special search tokens.
   *
   * @param {array} tokens
   *        An array of search tokens.
   * @returns {array} A new, filtered array of tokens.
   */
  filterTokens(tokens) {
    let foundToken = false;
    // Set the proper behavior while filtering tokens.
    let filtered = [];
    for (let token of tokens) {
      if (!UrlbarTokenizer.isRestrictionToken(token)) {
        filtered.push(token);
        continue;
      }
      let behavior = typeToBehaviorMap.get(token.type);
      if (!behavior) {
        throw new Error(`Unknown token type ${token.type}`);
      }
      // Don't use the suggest preferences if it is a token search and
      // set the restrict bit to 1 (to intersect the search results).
      if (!foundToken) {
        foundToken = true;
        // Do not take into account previous behavior (e.g.: history, bookmark)
        this._behavior = 0;
        this.setBehavior("restrict");
      }
      this.setBehavior(behavior);
      // We return tags only for bookmarks, thus when tags are enforced, we
      // must also set the bookmark behavior.
      if (behavior == "tag") {
        this.setBehavior("bookmark");
      }
    }
    return filtered;
  },

  /**
   * Stop this search.
   * After invoking this method, we won't run any more searches or heuristics,
   * and no new matches may be added to the current result.
   */
  stop() {
    // Avoid multiple calls or re-entrance.
    if (!this.pending) {
      return;
    }
    if (this._notifyTimer) {
      this._notifyTimer.cancel();
    }
    this._notifyDelaysCount = 0;
    if (typeof this.interrupt == "function") {
      this.interrupt();
    }
    this.pending = false;
  },

  /**
   * Whether this search is active.
   */
  pending: true,

  /**
   * Execute the search and populate results.
   * @param {mozIStorageAsyncConnection} conn
   *        The Sqlite connection.
   */
  async execute(conn) {
    // A search might be canceled before it starts.
    if (!this.pending) {
      return;
    }

    // Used by stop() to interrupt an eventual running statement.
    this.interrupt = () => {
      // Interrupt any ongoing statement to run the search sooner.
      if (!UrlbarProvidersManager.interruptLevel) {
        conn.interrupt();
      }
    };

    // For any given search, we run these queries:
    // 1) open pages not supported by history (this._switchToTabQuery)
    // 2) query based on match behavior

    // If the query is simply "@" and we have tokenAliasEngines then return
    // early. UrlbarProviderTokenAliasEngines will add engine results.
    let tokenAliasEngines = await UrlbarSearchUtils.tokenAliasEngines();
    if (this._trimmedOriginalSearchString == "@" && tokenAliasEngines.length) {
      this._provider.finishSearch(true);
      return;
    }

    // Check if the first token is an action. If it is, we should set a flag
    // so we don't include it in our searches.
    this._firstTokenIsKeyword =
      this._firstTokenIsKeyword || (await this._checkIfFirstTokenIsKeyword());
    if (!this.pending) {
      return;
    }

    if (this._trimmedOriginalSearchString) {
      // If the user typed the search restriction char or we're in
      // search-restriction mode, then we're done.
      // UrlbarProviderSearchSuggestions will handle suggestions, if any.
      let emptySearchRestriction =
        this._trimmedOriginalSearchString.length <= 3 &&
        this._leadingRestrictionToken == UrlbarTokenizer.RESTRICT.SEARCH &&
        /\s*\S?$/.test(this._trimmedOriginalSearchString);
      if (
        emptySearchRestriction ||
        (tokenAliasEngines &&
          this._trimmedOriginalSearchString.startsWith("@")) ||
        (this.hasBehavior("search") && this.hasBehavior("restrict"))
      ) {
        this._provider.finishSearch(true);
        return;
      }
    }

    // Run our standard Places query.
    let queries = [];
    // "openpage" behavior is supported by the default query.
    // _switchToTabQuery instead returns only pages not supported by history.
    if (this.hasBehavior("openpage")) {
      queries.push(this._switchToTabQuery);
    }
    queries.push(this._searchQuery);
    for (let [query, params] of queries) {
      await conn.executeCached(query, params, this._onResultRow.bind(this));
      if (!this.pending) {
        return;
      }
    }

    // If we do not have enough matches search again with MATCH_ANYWHERE, to
    // get more matches.
    let count = this._counts[MATCH_TYPE.GENERAL];
    if (count < this._maxResults) {
      this._matchBehavior = Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE;
      queries = [this._searchQuery];
      if (this.hasBehavior("openpage")) {
        queries.unshift(this._switchToTabQuery);
      }
      for (let [query, params] of queries) {
        await conn.executeCached(query, params, this._onResultRow.bind(this));
        if (!this.pending) {
          return;
        }
      }
    }
  },

  async _checkIfFirstTokenIsKeyword() {
    if (!this._heuristicToken) {
      return false;
    }

    let aliasEngine = await UrlbarSearchUtils.engineForAlias(
      this._heuristicToken,
      this._originalSearchString
    );

    if (aliasEngine) {
      return true;
    }

    let { entry } = await KeywordUtils.getBindableKeyword(
      this._heuristicToken,
      this._originalSearchString
    );
    if (entry) {
      this._filterOnHost = entry.url.host;
      return true;
    }

    return false;
  },

  /**
   * Adds a search engine match.
   *
   * @param {nsISearchEngine} engine
   *        The search engine associated with the match.
   * @param {string} [query]
   *        The search query string.
   * @param {string} [alias]
   *        The search engine alias associated with the match, if any.
   * @param {boolean} [historical]
   *        True if you're adding a suggestion match and the suggestion is from
   *        the user's local history (and not the search engine).
   */
  _addSearchEngineMatch({
    engine,
    query = "",
    alias = undefined,
    historical = false,
  }) {
    let actionURLParams = {
      engineName: engine.name,
      searchQuery: query,
    };

    if (alias && !query) {
      // `input` should have a trailing space so that when the user selects the
      // result, they can start typing their query without first having to enter
      // a space between the alias and query.
      actionURLParams.input = `${alias} `;
    } else {
      actionURLParams.input = this._originalSearchString;
    }

    let match = {
      comment: engine.name,
      icon: engine.iconURI ? engine.iconURI.spec : null,
      style: "action searchengine",
      frecency: FRECENCY_DEFAULT,
    };

    if (alias) {
      actionURLParams.alias = alias;
      match.style += " alias";
    }

    match.value = makeActionUrl("searchengine", actionURLParams);
    this._addMatch(match);
  },

  _onResultRow(row, cancel) {
    let queryType = row.getResultByIndex(QUERYINDEX_QUERYTYPE);
    switch (queryType) {
      case QUERYTYPE_FILTERED:
        this._addFilteredQueryMatch(row);
        break;
    }
    // If the search has been canceled by the user or by _addMatch, or we
    // fetched enough results, we can stop the underlying Sqlite query.
    let count = this._counts[MATCH_TYPE.GENERAL];
    if (!this.pending || count >= this._maxResults) {
      cancel();
    }
  },

  /**
   * Maybe restyle a SERP in history as a search-type result. To do this,
   * we extract the search term from the SERP in history then generate a search
   * URL with that search term. We restyle the SERP in history if its query
   * parameters are a subset of those of the generated SERP. We check for a
   * subset instead of exact equivalence since the generated URL may contain
   * attribution parameters while a SERP in history from an organic search would
   * not. We don't allow extra params in the history URL since they might
   * indicate the search is not a first-page web SERP (as opposed to a image or
   * other non-web SERP).
   *
   * @param {object} match
   * @returns {boolean} True if the match can be restyled, false otherwise.
   * @note We will mistakenly dedupe SERPs for engines that have the same
   *   hostname as another engine. One example is if the user installed a
   *   Google Image Search engine. That engine's search URLs might only be
   *   distinguished by query params from search URLs from the default Google
   *   engine.
   */
  _maybeRestyleSearchMatch(match) {
    // Return if the URL does not represent a search result.
    let historyUrl = match.value;
    let parseResult = Services.search.parseSubmissionURL(historyUrl);
    if (!parseResult?.engine) {
      return false;
    }

    // Here we check that the user typed all or part of the search string in the
    // search history result.
    let terms = parseResult.terms.toLowerCase();
    if (
      this._searchTokens.length &&
      this._searchTokens.every(token => !terms.includes(token.value))
    ) {
      return false;
    }

    // The URL for the search suggestion formed by the user's typed query.
    let [generatedSuggestionUrl] = UrlbarUtils.getSearchQueryUrl(
      parseResult.engine,
      this._searchTokens.map(t => t.value).join(" ")
    );

    // We ignore termsParameterName when checking for a subset because we
    // already checked that the typed query is a subset of the search history
    // query above with this._searchTokens.every(...).
    if (
      !UrlbarSearchUtils.serpsAreEquivalent(
        historyUrl,
        generatedSuggestionUrl,
        [parseResult.termsParameterName]
      )
    ) {
      return false;
    }

    // Turn the match into a searchengine action with a favicon.
    match.value = makeActionUrl("searchengine", {
      engineName: parseResult.engine.name,
      input: parseResult.terms,
      searchSuggestion: parseResult.terms,
      searchQuery: parseResult.terms,
      isSearchHistory: true,
    });
    match.comment = parseResult.engine.name;
    match.icon = match.icon || match.iconUrl;
    match.style = "action searchengine favicon suggestion";
    return true;
  },

  _addMatch(match) {
    if (typeof match.frecency != "number") {
      throw new Error("Frecency not provided");
    }

    if (typeof match.type != "string") {
      match.type = MATCH_TYPE.GENERAL;
    }

    // A search could be canceled between a query start and its completion,
    // in such a case ensure we won't notify any result for it.
    if (!this.pending) {
      return;
    }

    match.style = match.style || "favicon";

    // Restyle past searches, unless they are bookmarks or special results.
    if (
      match.style == "favicon" &&
      (UrlbarPrefs.get("restyleSearches") || this._searchModeEngine)
    ) {
      let restyled = this._maybeRestyleSearchMatch(match);
      if (restyled && UrlbarPrefs.get("maxHistoricalSearchSuggestions") == 0) {
        // The user doesn't want search history.
        return;
      }
    }

    match.icon = match.icon || "";
    match.finalCompleteValue = match.finalCompleteValue || "";

    let { index, replace } = this._getInsertIndexForMatch(match);
    if (index == -1) {
      return;
    }
    if (replace) {
      // Replacing an existing match from the previous search.
      this._matches.splice(index, 1);
    }
    this._matches.splice(index, 0, match);
    this._counts[match.type]++;

    this.notifyResult(true);
  },

  /**
   * Check for duplicates and either discard the duplicate or replace the
   * original match, in case the new one is more specific. For example,
   * a Remote Tab wins over History, and a Switch to Tab wins over a Remote Tab.
   * We must check both id and url for duplication, because keywords may change
   * the url by replacing the %s placeholder.
   * @param {object} match
   * @returns {object} matchPosition
   * @returns {number} matchPosition.index
   *   The index the match should take in the results. Return -1 if the match
   *   should be discarded.
   * @returns {boolean} matchPosition.replace
   *   True if the match should replace the result already at
   *   matchPosition.index.
   *
   */
  _getInsertIndexForMatch(match) {
    let [urlMapKey, prefix, action] = makeKeyForMatch(match);
    if (
      (match.placeId && this._usedPlaceIds.has(match.placeId)) ||
      this._usedURLs.some(e => ObjectUtils.deepEqual(e.key, urlMapKey))
    ) {
      let isDupe = true;
      if (action && ["switchtab", "remotetab"].includes(action.type)) {
        // The new entry is a switch/remote tab entry, look for the duplicate
        // among current matches.
        for (let i = 0; i < this._usedURLs.length; ++i) {
          let { key: matchKey, action: matchAction } = this._usedURLs[i];
          if (ObjectUtils.deepEqual(matchKey, urlMapKey)) {
            isDupe = true;
            if (!matchAction || action.type == "switchtab") {
              this._usedURLs[i] = {
                key: urlMapKey,
                action,
                type: match.type,
                prefix,
                comment: match.comment,
              };
              return { index: i, replace: true };
            }
            break; // Found the duplicate, no reason to continue.
          }
        }
      } else {
        // Dedupe with this flow:
        // 1. If the two URLs are the same, dedupe the newer one.
        // 2. If they both contain www. or both do not contain it, prefer https.
        // 3. If they differ by www., send both results to the Muxer and allow
        //    it to decide based on results from other providers.
        let prefixRank = UrlbarUtils.getPrefixRank(prefix);
        for (let i = 0; i < this._usedURLs.length; ++i) {
          if (!this._usedURLs[i]) {
            // This is true when the result at [i] is a searchengine result.
            continue;
          }

          let { key: existingKey, prefix: existingPrefix } = this._usedURLs[i];

          let existingPrefixRank = UrlbarUtils.getPrefixRank(existingPrefix);
          if (ObjectUtils.deepEqual(existingKey, urlMapKey)) {
            isDupe = true;

            if (prefix == existingPrefix) {
              // The URLs are identical. Throw out the new result.
              break;
            }

            if (prefix.endsWith("www.") == existingPrefix.endsWith("www.")) {
              // The results differ only by protocol.
              if (prefixRank <= existingPrefixRank) {
                break; // Replace match.
              } else {
                this._usedURLs[i] = {
                  key: urlMapKey,
                  action,
                  type: match.type,
                  prefix,
                  comment: match.comment,
                };
                return { index: i, replace: true };
              }
            } else {
              // We have two identical URLs that differ only by www. We need to
              // be sure what the heuristic result is before deciding how we
              // should dedupe. We mark these as non-duplicates and let the
              // muxer handle it.
              isDupe = false;
              continue;
            }
          }
        }
      }

      // Discard the duplicate.
      if (isDupe) {
        return { index: -1, replace: false };
      }
    }

    // Add this to our internal tracker to ensure duplicates do not end up in
    // the result.
    // Not all entries have a place id, thus we fallback to the url for them.
    // We cannot use only the url since keywords entries are modified to
    // include the search string, and would be returned multiple times.  Ids
    // are faster too.
    if (match.placeId) {
      this._usedPlaceIds.add(match.placeId);
    }

    let index = 0;
    if (!this._buckets) {
      this._buckets = [];
      this._makeBuckets(UrlbarPrefs.get("resultGroups"), this._maxResults);
    }

    let replace = 0;
    for (let bucket of this._buckets) {
      // Move to the next bucket if the match type is incompatible, or if there
      // is no available space or if the frecency is below the threshold.
      if (match.type != bucket.type || !bucket.available) {
        index += bucket.count;
        continue;
      }

      index += bucket.insertIndex;
      bucket.available--;
      if (bucket.insertIndex < bucket.count) {
        replace = true;
      } else {
        bucket.count++;
      }
      bucket.insertIndex++;
      break;
    }
    this._usedURLs[index] = {
      key: urlMapKey,
      action,
      type: match.type,
      prefix,
      comment: match.comment || "",
    };
    return { index, replace };
  },

  _makeBuckets(resultBucket, maxResultCount) {
    if (!resultBucket.children) {
      let type;
      switch (resultBucket.group) {
        case UrlbarUtils.RESULT_GROUP.FORM_HISTORY:
        case UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION:
        case UrlbarUtils.RESULT_GROUP.TAIL_SUGGESTION:
          type = MATCH_TYPE.SUGGESTION;
          break;
        case UrlbarUtils.RESULT_GROUP.HEURISTIC_AUTOFILL:
        case UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION:
        case UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK:
        case UrlbarUtils.RESULT_GROUP.HEURISTIC_OMNIBOX:
        case UrlbarUtils.RESULT_GROUP.HEURISTIC_SEARCH_TIP:
        case UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST:
        case UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE:
          type = MATCH_TYPE.HEURISTIC;
          break;
        case UrlbarUtils.RESULT_GROUP.OMNIBOX:
          type = MATCH_TYPE.EXTENSION;
          break;
        default:
          type = MATCH_TYPE.GENERAL;
          break;
      }
      if (this._buckets.length) {
        let last = this._buckets[this._buckets.length - 1];
        if (last.type == type) {
          return;
        }
      }
      // - `available` is the number of available slots in the bucket
      // - `insertIndex` is the index of the first available slot in the bucket
      // - `count` is the number of matches in the bucket, note that it also
      //   accounts for matches from the previous search, while `available` and
      //   `insertIndex` don't.
      this._buckets.push({
        type,
        available: maxResultCount,
        insertIndex: 0,
        count: 0,
      });
      return;
    }

    let initialMaxResultCount;
    if (typeof resultBucket.maxResultCount == "number") {
      initialMaxResultCount = resultBucket.maxResultCount;
    } else if (typeof resultBucket.availableSpan == "number") {
      initialMaxResultCount = resultBucket.availableSpan;
    } else {
      initialMaxResultCount = this._maxResults;
    }
    let childMaxResultCount = Math.min(initialMaxResultCount, maxResultCount);
    for (let child of resultBucket.children) {
      this._makeBuckets(child, childMaxResultCount);
    }
  },

  _addFilteredQueryMatch(row) {
    let placeId = row.getResultByIndex(QUERYINDEX_PLACEID);
    let url = row.getResultByIndex(QUERYINDEX_URL);
    let openPageCount = row.getResultByIndex(QUERYINDEX_SWITCHTAB) || 0;
    let historyTitle = row.getResultByIndex(QUERYINDEX_TITLE) || "";
    let bookmarked = row.getResultByIndex(QUERYINDEX_BOOKMARKED);
    let bookmarkTitle = bookmarked
      ? row.getResultByIndex(QUERYINDEX_BOOKMARKTITLE)
      : null;
    let tags = row.getResultByIndex(QUERYINDEX_TAGS) || "";
    let frecency = row.getResultByIndex(QUERYINDEX_FRECENCY);

    let match = {
      placeId,
      value: url,
      comment: bookmarkTitle || historyTitle,
      icon: UrlbarUtils.getIconForUrl(url),
      frecency: frecency || FRECENCY_DEFAULT,
    };

    if (openPageCount > 0 && this.hasBehavior("openpage")) {
      if (this._currentPage == match.value) {
        // Don't suggest switching to the current tab.
        return;
      }
      // Actions are enabled and the page is open.  Add a switch-to-tab result.
      match.value = makeActionUrl("switchtab", { url: match.value });
      match.style = "action switchtab";
    } else if (
      this.hasBehavior("history") &&
      !this.hasBehavior("bookmark") &&
      !tags
    ) {
      // The consumer wants only history and not bookmarks and there are no
      // tags.  We'll act as if the page is not bookmarked.
      match.style = "favicon";
    } else if (tags) {
      // Store the tags in the title.  It's up to the consumer to extract them.
      match.comment += UrlbarUtils.TITLE_TAGS_SEPARATOR + tags;
      // If we're not suggesting bookmarks, then this shouldn't display as one.
      match.style = this.hasBehavior("bookmark") ? "bookmark-tag" : "tag";
    } else if (bookmarked) {
      match.style = "bookmark";
    }

    this._addMatch(match);
  },

  /**
   * @returns {string}
   * A string consisting of the search query to be used based on the previously
   * set urlbar suggestion preferences.
   */
  get _suggestionPrefQuery() {
    let conditions = [];
    if (this._filterOnHost) {
      conditions.push("h.rev_host = get_unreversed_host(:host || '.') || '.'");
      // When filtering on a host we are in some sort of site specific search,
      // thus we want a cleaner set of results, compared to a general search.
      // This means removing less interesting urls, like redirects or
      // non-bookmarked title-less pages.

      if (UrlbarPrefs.get("restyleSearches") || this._searchModeEngine) {
        // If restyle is enabled, we want to filter out redirect targets,
        // because sources are urls built using search engines definitions that
        // we can reverse-parse.
        // In this case we can't filter on title-less pages because redirect
        // sources likely don't have a title and recognizing sources is costly.
        // Bug 468710 may help with this.
        conditions.push(`NOT EXISTS (
          WITH visits(type) AS (
            SELECT visit_type
            FROM moz_historyvisits
            WHERE place_id = h.id
            ORDER BY visit_date DESC
            LIMIT 10 /* limit to the last 10 visits */
          )
          SELECT 1 FROM visits
          WHERE type IN (5,6)
        )`);
      } else {
        // If instead restyle is disabled, we want to keep redirect targets,
        // because sources are often unreadable title-less urls.
        conditions.push(`NOT EXISTS (
          WITH visits(id) AS (
            SELECT id
            FROM moz_historyvisits
            WHERE place_id = h.id
            ORDER BY visit_date DESC
            LIMIT 10 /* limit to the last 10 visits */
            )
           SELECT 1
           FROM visits src
           JOIN moz_historyvisits dest ON src.id = dest.from_visit
           WHERE dest.visit_type IN (5,6)
        )`);
        // Filter out empty-titled pages, they could be redirect sources that
        // we can't recognize anymore because their target was wrongly expired
        // due to Bug 1664252.
        conditions.push("(h.foreign_count > 0 OR h.title NOTNULL)");
      }
    }

    if (
      this.hasBehavior("restrict") ||
      !this.hasBehavior("history") ||
      !this.hasBehavior("bookmark")
    ) {
      if (this.hasBehavior("history")) {
        // Enforce ignoring the visit_count index, since the frecency one is much
        // faster in this case.  ANALYZE helps the query planner to figure out the
        // faster path, but it may not have up-to-date information yet.
        conditions.push("+h.visit_count > 0");
      }
      if (this.hasBehavior("bookmark")) {
        conditions.push("bookmarked");
      }
      if (this.hasBehavior("tag")) {
        conditions.push("tags NOTNULL");
      }
    }

    return defaultQuery(conditions.join(" AND "));
  },

  get _emptySearchDefaultBehavior() {
    // Further restrictions to apply for "empty searches" (searching for
    // "").  The empty behavior is typed history, if history is enabled.
    // Otherwise, it is bookmarks, if they are enabled. If both history and
    // bookmarks are disabled, it defaults to open pages.
    let val = Ci.mozIPlacesAutoComplete.BEHAVIOR_RESTRICT;
    if (UrlbarPrefs.get("suggest.history")) {
      val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_HISTORY;
    } else if (UrlbarPrefs.get("suggest.bookmark")) {
      val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_BOOKMARK;
    } else {
      val |= Ci.mozIPlacesAutoComplete.BEHAVIOR_OPENPAGE;
    }
    return val;
  },

  /**
   * If the user-provided string starts with a keyword that gave a heuristic
   * result, this will strip it.
   * @returns {string} The filtered search string.
   */
  get _keywordFilteredSearchString() {
    let tokens = this._searchTokens.map(t => t.value);
    if (this._firstTokenIsKeyword) {
      tokens = tokens.slice(1);
    }
    return tokens.join(" ");
  },

  /**
   * Obtains the search query to be used based on the previously set search
   * preferences (accessed by this.hasBehavior).
   *
   * @returns {array}
   *   An array consisting of the correctly optimized query to search the
   *   database with and an object containing the params to bound.
   */
  get _searchQuery() {
    let params = {
      parent: PlacesUtils.tagsFolderId,
      query_type: QUERYTYPE_FILTERED,
      matchBehavior: this._matchBehavior,
      searchBehavior: this._behavior,
      // We only want to search the tokens that we are left with - not the
      // original search string.
      searchString: this._keywordFilteredSearchString,
      userContextId: this._userContextId,
      // Limit the query to the the maximum number of desired results.
      // This way we can avoid doing more work than needed.
      maxResults: this._maxResults,
    };
    if (this._filterOnHost) {
      params.host = this._filterOnHost;
    }
    return [this._suggestionPrefQuery, params];
  },

  /**
   * Obtains the query to search for switch-to-tab entries.
   *
   * @returns {array}
   *   An array consisting of the correctly optimized query to search the
   *   database with and an object containing the params to bound.
   */
  get _switchToTabQuery() {
    return [
      SQL_SWITCHTAB_QUERY,
      {
        query_type: QUERYTYPE_FILTERED,
        matchBehavior: this._matchBehavior,
        searchBehavior: this._behavior,
        // We only want to search the tokens that we are left with - not the
        // original search string.
        searchString: this._keywordFilteredSearchString,
        userContextId: this._userContextId,
        maxResults: this._maxResults,
      },
    ];
  },

  // The result is notified to the search listener on a timer, to chunk multiple
  // match updates together and avoid rebuilding the popup at every new match.
  _notifyTimer: null,

  /**
   * Notifies the current result to the listener.
   *
   * @param searchOngoing
   *        Indicates whether the search result should be marked as ongoing.
   */
  _notifyDelaysCount: 0,
  notifyResult(searchOngoing) {
    let notify = () => {
      if (!this.pending) {
        return;
      }
      this._notifyDelaysCount = 0;
      this._listener(this._matches, searchOngoing);
      if (!searchOngoing) {
        // Break possible cycles.
        this._listener = null;
        this._provider = null;
        this.stop();
      }
    };
    if (this._notifyTimer) {
      this._notifyTimer.cancel();
    }
    // In the worst case, we may get evenly spaced matches that would end up
    // delaying the UI by N_MATCHES * NOTIFYRESULT_DELAY_MS. Thus, we clamp the
    // number of times we may delay matches.
    if (this._notifyDelaysCount > 3) {
      notify();
    } else {
      this._notifyDelaysCount++;
      this._notifyTimer = setTimeout(notify, NOTIFYRESULT_DELAY_MS);
    }
  },
};

/**
 * Class used to create the provider.
 */
class ProviderPlaces extends UrlbarProvider {
  // Promise resolved when the database initialization has completed, or null
  // if it has never been requested.
  _promiseDatabase = null;

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "Places";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Gets a Sqlite database handle.
   *
   * @returns {Promise}
   * @resolves to the Sqlite database handle (according to Sqlite.jsm).
   * @rejects javascript exception.
   */
  getDatabaseHandle() {
    if (!this._promiseDatabase) {
      this._promiseDatabase = (async () => {
        let conn = await PlacesUtils.promiseLargeCacheDBConnection();

        // We don't catch exceptions here as it is too late to block shutdown.
        Sqlite.shutdown.addBlocker("UrlbarProviderPlaces closing", () => {
          // Break a possible cycle through the
          // previous result, the controller and
          // ourselves.
          this._currentSearch = null;
        });

        return conn;
      })().catch(ex => {
        dump("Couldn't get database handle: " + ex + "\n");
        Cu.reportError(ex);
      });
    }
    return this._promiseDatabase;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    if (
      !queryContext.trimmedSearchString &&
      queryContext.searchMode?.engineName &&
      UrlbarPrefs.get("update2.emptySearchBehavior") < 2
    ) {
      return false;
    }
    return true;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   * @returns {Promise} resolved when the query stops.
   */
  startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;
    let urls = new Set();
    this._startLegacyQuery(queryContext, matches => {
      if (instance != this.queryInstance) {
        return;
      }
      let results = convertLegacyMatches(queryContext, matches, urls);
      for (let result of results) {
        addCallback(this, result);
      }
    });
    return this._deferred.promise;
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    if (this._currentSearch) {
      this._currentSearch.stop();
    }
    if (this._deferred) {
      this._deferred.resolve();
    }
    // Don't notify since we are canceling this search.  This also means we
    // won't fire onSearchComplete for this search.
    this.finishSearch();
  }

  /**
   * Properly cleans up when searching is completed.
   *
   * @param {boolean} [notify]
   *        Indicates if we should notify the AutoComplete listener about our
   *        results or not. Default false.
   */
  finishSearch(notify = false) {
    // Clear state now to avoid race conditions, see below.
    let search = this._currentSearch;
    if (!search) {
      return;
    }
    this._lastLowResultsSearchSuggestion =
      search._lastLowResultsSearchSuggestion;

    if (!notify || !search.pending) {
      return;
    }

    // There is a possible race condition here.
    // When a search completes it calls finishSearch that notifies results
    // here.  When the controller gets the last result it fires
    // onSearchComplete.
    // If onSearchComplete immediately starts a new search it will set a new
    // _currentSearch, and on return the execution will continue here, after
    // notifyResult.
    // Thus, ensure that notifyResult is the last call in this method,
    // otherwise you might be touching the wrong search.
    search.notifyResult(false);
  }

  _startLegacyQuery(queryContext, callback) {
    let deferred = PromiseUtils.defer();
    let listener = (matches, searchOngoing) => {
      callback(matches);
      if (!searchOngoing) {
        deferred.resolve();
      }
    };
    this._startSearch(queryContext.searchString, listener, queryContext);
    this._deferred = deferred;
  }

  _startSearch(searchString, listener, queryContext) {
    // Stop the search in case the controller has not taken care of it.
    if (this._currentSearch) {
      this.cancelQuery();
    }

    let search = (this._currentSearch = new Search(
      queryContext,
      listener,
      this
    ));
    this.getDatabaseHandle()
      .then(conn => search.execute(conn))
      .catch(ex => {
        dump(`Query failed: ${ex}\n`);
        Cu.reportError(ex);
      })
      .then(() => {
        if (search == this._currentSearch) {
          this.finishSearch(true);
        }
      });
  }
}

var UrlbarProviderPlaces = new ProviderPlaces();
