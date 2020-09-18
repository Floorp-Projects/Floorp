/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers search engine suggestions.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderSearchSuggestions"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SkippableTimer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Returns whether the passed in string looks like a url.
 * @param {string} str
 * @param {boolean} [ignoreAlphanumericHosts]
 * @returns {boolean}
 *   True if the query looks like a URL.
 */
function looksLikeUrl(str, ignoreAlphanumericHosts = false) {
  // Single word including special chars.
  return (
    !UrlbarTokenizer.REGEXP_SPACES.test(str) &&
    (["/", "@", ":", "["].some(c => str.includes(c)) ||
      (ignoreAlphanumericHosts
        ? /^([\[\]A-Z0-9-]+\.){3,}[^.]+$/i.test(str)
        : str.includes(".")))
  );
}

/**
 * Class used to create the provider.
 */
class ProviderSearchSuggestions extends UrlbarProvider {
  constructor() {
    super();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "SearchSuggestions";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.NETWORK;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    // If the sources don't include search or the user used a restriction
    // character other than search, don't allow any suggestions.
    if (
      !queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.SEARCH) ||
      (queryContext.restrictSource &&
        queryContext.restrictSource != UrlbarUtils.RESULT_SOURCE.SEARCH)
    ) {
      return false;
    }

    // No suggestions for empty search strings, unless we are restricting to
    // search.
    if (
      !queryContext.trimmedSearchString &&
      !this._isTokenOrRestrictionPresent(queryContext)
    ) {
      return false;
    }

    if (!this._allowSuggestions(queryContext)) {
      return false;
    }

    let wantsLocalSuggestions =
      UrlbarPrefs.get("maxHistoricalSearchSuggestions") &&
      (!UrlbarPrefs.get("update2") ||
        queryContext.trimmedSearchString ||
        UrlbarPrefs.get("update2.emptySearchBehavior") != 0);

    return wantsLocalSuggestions || this._allowRemoteSuggestions(queryContext);
  }

  /**
   * Returns whether the user typed a token alias or restriction token, or is in
   * search mode. We use this value to override the pref to disable search
   * suggestions in the Urlbar.
   * @param {UrlbarQueryContext} queryContext  The query context object.
   * @returns {boolean} True if the user typed a token alias or search
   *   restriction token.
   */
  _isTokenOrRestrictionPresent(queryContext) {
    return (
      queryContext.searchString.startsWith("@") ||
      (queryContext.restrictSource &&
        queryContext.restrictSource == UrlbarUtils.RESULT_SOURCE.SEARCH) ||
      queryContext.tokens.some(
        t => t.type == UrlbarTokenizer.TYPE.RESTRICT_SEARCH
      ) ||
      (queryContext.searchMode &&
        queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.SEARCH))
    );
  }

  /**
   * Returns whether suggestions in general are allowed for a given query
   * context.  If this returns false, then we shouldn't fetch either form
   * history or remote suggestions.
   *
   * @param {object} queryContext The query context object
   * @returns {boolean} True if suggestions in general are allowed and false if
   *   not.
   */
  _allowSuggestions(queryContext) {
    if (
      !queryContext.allowSearchSuggestions ||
      // If the user typed a restriction token or token alias, we ignore the
      // pref to disable suggestions in the Urlbar.
      (!UrlbarPrefs.get("suggest.searches") &&
        !this._isTokenOrRestrictionPresent(queryContext)) ||
      !UrlbarPrefs.get("browser.search.suggest.enabled") ||
      (queryContext.isPrivate &&
        !UrlbarPrefs.get("browser.search.suggest.enabled.private"))
    ) {
      return false;
    }
    return true;
  }

  /**
   * Returns whether remote suggestions are allowed for a given query context.
   *
   * @param {object} queryContext The query context object
   * @param {string} [searchString] The effective search string without
   *        restriction tokens or aliases. Defaults to the context searchString.
   * @returns {boolean} True if remote suggestions are allowed and false if not.
   */
  _allowRemoteSuggestions(
    queryContext,
    searchString = queryContext.searchString
  ) {
    // TODO (Bug 1626964): Support zero prefix suggestions.
    if (!searchString.trim()) {
      return false;
    }

    // Skip all remaining checks and allow remote suggestions at this point if
    // the user used a token alias or restriction token. We want "@engine query"
    // to return suggestions from the engine. We'll return early from startQuery
    // if the query doesn't match an alias.
    if (this._isTokenOrRestrictionPresent(queryContext)) {
      return true;
    }

    // If the user is just adding on to a query that previously didn't return
    // many remote suggestions, we are unlikely to get any more results.
    if (
      !!this._lastLowResultsSearchSuggestion &&
      searchString.length > this._lastLowResultsSearchSuggestion.length &&
      searchString.startsWith(this._lastLowResultsSearchSuggestion)
    ) {
      return false;
    }

    // We're unlikely to get useful remote suggestions for a single character.
    if (searchString.length < 2) {
      return false;
    }

    // Disallow remote suggestions if only an origin is typed to avoid
    // disclosing information about sites the user visits. This also catches
    // partially-typed origins, like mozilla.o, because the URIFixup check
    // below can't validate those.
    if (
      queryContext.tokens.length == 1 &&
      queryContext.tokens[0].type == UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN
    ) {
      return false;
    }

    // Disallow remote suggestions for strings containing tokens that look like
    // URIs, to avoid disclosing information about networks or passwords.
    if (queryContext.fixupInfo?.href && !queryContext.fixupInfo?.isSearch) {
      return false;
    }

    // Allow remote suggestions.
    return true;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;

    let aliasEngine = await this._maybeGetAlias(queryContext);
    if (!aliasEngine) {
      // Autofill matches queries starting with "@" to token alias engines.
      // If the string starts with "@", but an alias engine is not yet
      // matched, then autofill might still be filtering token alias
      // engine results. We don't want to mix search suggestions with those
      // engine results, so we return early. See bug 1551049 comment 1 for
      // discussion on how to improve this behavior.
      if (queryContext.searchString.startsWith("@")) {
        return;
      }
    }

    let query = aliasEngine
      ? aliasEngine.query
      : UrlbarUtils.substringAt(
          queryContext.searchString,
          queryContext.tokens[0]?.value || ""
        ).trim();

    let leadingRestrictionToken = null;
    if (
      UrlbarTokenizer.isRestrictionToken(queryContext.tokens[0]) &&
      (queryContext.tokens.length > 1 ||
        queryContext.tokens[0].type == UrlbarTokenizer.TYPE.RESTRICT_SEARCH)
    ) {
      leadingRestrictionToken = queryContext.tokens[0].value;
    }

    // Strip a leading search restriction char, because we prepend it to text
    // when the search shortcut is used and it's not user typed. Don't strip
    // other restriction chars, so that it's possible to search for things
    // including one of those (e.g. "c#").
    if (leadingRestrictionToken === UrlbarTokenizer.RESTRICT.SEARCH) {
      query = UrlbarUtils.substringAfter(query, leadingRestrictionToken).trim();
    }

    // Find our search engine. It may have already been set with an alias.
    let engine;
    if (aliasEngine) {
      engine = aliasEngine.engine;
    } else if (queryContext.searchMode?.engineName) {
      engine = Services.search.getEngineByName(
        queryContext.searchMode.engineName
      );
    } else {
      engine = UrlbarSearchUtils.getDefaultEngine(queryContext.isPrivate);
    }

    if (!engine) {
      return;
    }

    let alias = (aliasEngine && aliasEngine.alias) || "";
    let results = await this._fetchSearchSuggestions(
      queryContext,
      engine,
      query,
      alias
    );

    if (!results || instance != this.queryInstance) {
      return;
    }

    for (let result of results) {
      addCallback(this, result);
    }
  }

  /**
   * Gets the provider's priority.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   */
  getPriority(queryContext) {
    return 0;
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    if (this._suggestionsController) {
      this._suggestionsController.stop();
      this._suggestionsController = null;
    }
  }

  async _fetchSearchSuggestions(queryContext, engine, searchString, alias) {
    if (!engine) {
      return null;
    }

    this._suggestionsController = new SearchSuggestionController();
    this._suggestionsController.formHistoryParam = queryContext.formHistoryName;

    // If there's a form history entry that equals the search string, the search
    // suggestions controller will include it, and we'll make a result for it.
    // If the heuristic result ends up being a search result, the muxer will
    // discard the form history result since it dupes the heuristic, and the
    // final list of results would be left with `count` - 1 form history results
    // instead of `count`.  Therefore we request `count` + 1 entries.  The muxer
    // will dedupe and limit the final form history count as appropriate.
    this._suggestionsController.maxLocalResults = queryContext.maxResults + 1;

    // Request maxResults + 1 remote suggestions for the same reason we request
    // maxHistoricalSearchSuggestions + 1 form history entries.
    let allowRemote = this._allowRemoteSuggestions(queryContext, searchString);
    this._suggestionsController.maxRemoteResults = allowRemote
      ? queryContext.maxResults + 1
      : 0;

    this._suggestionsFetchCompletePromise = this._suggestionsController.fetch(
      searchString,
      queryContext.isPrivate,
      engine,
      queryContext.userContextId,
      this._isTokenOrRestrictionPresent(queryContext),
      false
    );

    // See `SearchSuggestionsController.fetch` documentation for a description
    // of `fetchData`.
    let fetchData = await this._suggestionsFetchCompletePromise;
    // The fetch was canceled.
    if (!fetchData) {
      return null;
    }

    let results = [];

    // We use this to discard remote suggestions that duplicate form history.
    let seenSuggestions = new Set();

    // Add maxHistoricalSearchSuggestions form history results to the beginning
    // of the array.  Below we'll add the remainder after remote suggestions.
    // Any excess results that are not discarded by the muxer will appear below
    // the remote suggestions in the final results.
    let maxInitialFormHistory = UrlbarPrefs.get(
      "maxHistoricalSearchSuggestions"
    );
    while (results.length < maxInitialFormHistory && fetchData.local.length) {
      let entry = fetchData.local.shift();
      results.push(makeFormHistoryResult(queryContext, engine, entry));
      seenSuggestions.add(entry.value);
    }

    // If we don't return many results, then keep track of the query. If the
    // user just adds on to the query, we won't fetch more suggestions if the
    // query is very long since we are unlikely to get any.
    if (
      allowRemote &&
      !fetchData.remote.length &&
      searchString.length > UrlbarPrefs.get("maxCharsForSearchSuggestions")
    ) {
      this._lastLowResultsSearchSuggestion = searchString;
    }

    // If we have only tail suggestions, we only show them if we have no other
    // results. We need to wait for other results to arrive to avoid flickering.
    // We will wait for this timer unless we have suggestions that don't have a
    // tail.
    let tailTimer = new SkippableTimer({
      name: "ProviderSearchSuggestions",
      time: 100,
      logger: this.logger,
    });

    for (let entry of fetchData.remote) {
      if (looksLikeUrl(entry.value) || seenSuggestions.has(entry.value)) {
        continue;
      }

      if (entry.tail && entry.tailOffsetIndex < 0) {
        Cu.reportError(
          `Error in tail suggestion parsing. Value: ${entry.value}, tail: ${entry.tail}.`
        );
        continue;
      }

      let tail = entry.tail;
      let tailPrefix = entry.matchPrefix;

      // Skip tail suggestions if the pref is disabled.
      if (tail && !UrlbarPrefs.get("richSuggestions.tail")) {
        continue;
      }

      if (!tail) {
        await tailTimer.fire().catch(Cu.reportError);
      }

      try {
        results.push(
          new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.SEARCH,
            UrlbarUtils.RESULT_SOURCE.SEARCH,
            ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
              engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
              suggestion: [entry.value, UrlbarUtils.HIGHLIGHT.SUGGESTED],
              lowerCaseSuggestion: entry.value.toLocaleLowerCase(),
              tailPrefix,
              tail: [tail, UrlbarUtils.HIGHLIGHT.SUGGESTED],
              tailOffsetIndex: tail ? entry.tailOffsetIndex : undefined,
              keyword: [alias ? alias : undefined, UrlbarUtils.HIGHLIGHT.TYPED],
              query: [searchString.trim(), UrlbarUtils.HIGHLIGHT.NONE],
              icon: !entry.value ? engine.iconURI?.spec : undefined,
            })
          )
        );
        seenSuggestions.add(entry.value);
      } catch (err) {
        Cu.reportError(err);
        continue;
      }
    }

    // Add the remaining form history results.
    while (
      results.length < queryContext.maxResults + 1 &&
      fetchData.local.length
    ) {
      let entry = fetchData.local.shift();
      if (!seenSuggestions.has(entry.value)) {
        results.push(makeFormHistoryResult(queryContext, engine, entry));
      }
    }

    await tailTimer.promise;
    return results;
  }

  /**
   * Searches for an engine alias given the queryContext.
   * @param {UrlbarQueryContext} queryContext
   * @returns {object} aliasEngine
   *   A representation of the aliased engine. Null if there's no match.
   * @returns {nsISearchEngine} aliasEngine.engine
   * @returns {string} aliasEngine.alias
   * @returns {string} aliasEngine.query
   * @returns {object} { engine, alias, query }
   *
   */
  async _maybeGetAlias(queryContext) {
    if (queryContext.searchMode) {
      // If we're in search mode, don't try to parse an alias at all.
      return null;
    }

    let possibleAlias = queryContext.tokens[0]?.value;
    // "@" on its own is handled by UrlbarProviderTokenAliasEngines and returns
    // a list of every available token alias.
    if (!possibleAlias || possibleAlias == "@") {
      return null;
    }

    let query = UrlbarUtils.substringAfter(
      queryContext.searchString,
      possibleAlias
    );

    // Match an alias only when it has a space after it.  If there's no trailing
    // space, then continue to treat it as part of the search string.
    if (UrlbarPrefs.get("update2") && !query.startsWith(" ")) {
      return null;
    }

    // Check if the user entered an engine alias directly.
    let engineMatch = await UrlbarSearchUtils.engineForAlias(possibleAlias);
    if (engineMatch) {
      return {
        engine: engineMatch,
        alias: possibleAlias,
        query: query.trim(),
      };
    }

    return null;
  }
}

function makeFormHistoryResult(queryContext, engine, entry) {
  return new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      engine: engine.name,
      suggestion: [entry.value, UrlbarUtils.HIGHLIGHT.SUGGESTED],
      lowerCaseSuggestion: entry.value.toLocaleLowerCase(),
    })
  );
}

var UrlbarProviderSearchSuggestions = new ProviderSearchSuggestions();
