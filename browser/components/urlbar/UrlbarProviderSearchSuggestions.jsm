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
  Log: "resource://gre/modules/Log.jsm",
  PlacesSearchAutocompleteProvider:
    "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SkippableTimer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Provider.SearchSuggestions")
);

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
 * Returns the portion of a string starting at the index where another string
 * begins.
 *
 * @param   {string} sourceStr
 *          The string to search within.
 * @param   {string} targetStr
 *          The string to search for.
 * @returns {string} The substring within sourceStr starting at targetStr, or
 *          the empty string if targetStr does not occur in sourceStr.
 */
function substringAt(sourceStr, targetStr) {
  let index = sourceStr.indexOf(targetStr);
  return index < 0 ? "" : sourceStr.substr(index);
}

/**
 * Returns the portion of a string starting at the index where another string
 * ends.
 *
 * @param   {string} sourceStr
 *          The string to search within.
 * @param   {string} targetStr
 *          The string to search for.
 * @returns {string} The substring within sourceStr where targetStr ends, or the
 *          empty string if targetStr does not occur in sourceStr.
 */
function substringAfter(sourceStr, targetStr) {
  let index = sourceStr.indexOf(targetStr);
  return index < 0 ? "" : sourceStr.substr(index + targetStr.length);
}

/**
 * Class used to create the provider.
 */
class ProviderSearchSuggestions extends UrlbarProvider {
  constructor() {
    super();
    // Maps the running queries by queryContext.
    this.queries = new Map();
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

    // No suggestions for empty search strings.
    if (!queryContext.searchString.trim()) {
      return false;
    }

    return (
      this._getFormHistoryCount(queryContext) ||
      this._allowRemoteSuggestions(queryContext)
    );
  }

  /**
   * Returns whether suggestions in general are allowed for a given query
   * context.  If this returns false, then we shouldn't fetch either form
   * history or remote suggestions.  Otherwise further checks are necessary to
   * determine whether to allow either form history or remote suggestions; see
   * _getFormHistoryCount and _allowRemoteSuggestions.
   *
   * @param {object} queryContext The query context object
   * @returns {boolean} True if suggestions in general are allowed and false if
   *   not.
   */
  _allowSuggestions(queryContext) {
    if (
      !queryContext.allowSearchSuggestions ||
      !UrlbarPrefs.get("suggest.searches") ||
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
   * @returns {boolean} True if remote suggestions are allowed and false if not.
   */
  _allowRemoteSuggestions(queryContext) {
    // Check whether suggestions in general are allowed.
    if (!this._allowSuggestions(queryContext)) {
      return false;
    }

    // Skip all remaining checks and allow remote suggestions at this point if
    // the user used a search engine token alias.  We want "@engine query" to
    // return suggestions from the engine.  We'll return early from startQuery
    // if the query doesn't match an alias.
    if (queryContext.searchString.startsWith("@")) {
      return true;
    }

    // If the user is just adding on to a query that previously didn't return
    // many remote suggestions, we are unlikely to get any more results.
    if (
      !!this._lastLowResultsSearchSuggestion &&
      queryContext.searchString.length >
        this._lastLowResultsSearchSuggestion.length &&
      queryContext.searchString.startsWith(this._lastLowResultsSearchSuggestion)
    ) {
      return false;
    }

    // We're unlikely to get useful remote suggestions for single characters.
    if (queryContext.searchString.length < 2) {
      return false;
    }

    // Disallow remote suggestions if only an origin is typed to avoid
    // disclosing information about sites the user visits.
    if (
      queryContext.tokens.length == 1 &&
      queryContext.tokens[0].type == UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN
    ) {
      return false;
    }

    // Disallow remote suggestions for strings containing tokens that look like
    // URLs or non-alphanumeric origins, to avoid disclosing information about
    // networks or passwords.
    if (
      queryContext.tokens.some(
        t =>
          t.type == UrlbarTokenizer.TYPE.POSSIBLE_URL ||
          (t.type == UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN &&
            !UrlbarTokenizer.REGEXP_SINGLE_WORD_HOST.test(t.value))
      )
    ) {
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
    logger.info(`Starting query for ${queryContext.searchString}`);
    let instance = {};
    this.queries.set(queryContext, instance);

    let trimmedOriginalSearchString = queryContext.searchString.trim();

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
      : substringAt(queryContext.searchString, queryContext.tokens[0].value);
    if (!query) {
      return;
    }

    let leadingRestrictionToken = null;
    if (
      UrlbarTokenizer.isRestrictionToken(queryContext.tokens[0]) &&
      (queryContext.tokens.length > 1 ||
        queryContext.tokens[0].type == UrlbarTokenizer.TYPE.RESTRICT_SEARCH)
    ) {
      leadingRestrictionToken = queryContext.tokens[0].value;
    }

    // If the heuristic result is a search engine result with an empty query
    // and we have either a token alias or the search restriction char, then
    // we're done.
    // For the restriction character case, also consider a single char query
    // or just the char itself, anyway we don't return search suggestions
    // unless at least 2 chars have been typed. Thus "?__" and "? a" should
    // finish here, while "?aa" should continue.
    let emptyQueryTokenAlias =
      aliasEngine && aliasEngine.isTokenAlias && !aliasEngine.query;
    let emptySearchRestriction =
      trimmedOriginalSearchString.length <= 3 &&
      leadingRestrictionToken == UrlbarTokenizer.RESTRICT.SEARCH &&
      /\s*\S?$/.test(trimmedOriginalSearchString);
    if (emptySearchRestriction || emptyQueryTokenAlias) {
      return;
    }

    // Strip a leading search restriction char, because we prepend it to text
    // when the search shortcut is used and it's not user typed. Don't strip
    // other restriction chars, so that it's possible to search for things
    // including one of those (e.g. "c#").
    if (leadingRestrictionToken === UrlbarTokenizer.RESTRICT.SEARCH) {
      query = substringAfter(query, leadingRestrictionToken).trim();
    }

    // Find our search engine. It may have already been set with an alias.
    let engine;
    if (aliasEngine) {
      engine = aliasEngine.engine;
    } else {
      engine = queryContext.engineName
        ? Services.search.getEngineByName(queryContext.engineName)
        : await PlacesSearchAutocompleteProvider.currentEngine(
            queryContext.isPrivate
          );
      if (!engine) {
        return;
      }
    }

    let alias = (aliasEngine && aliasEngine.alias) || "";
    let results = await this._fetchSearchSuggestions(
      queryContext,
      engine,
      query,
      alias
    );

    if (!results || !this.queries.has(queryContext)) {
      return;
    }

    for (let result of results) {
      addCallback(this, result);
    }

    this.queries.delete(queryContext);
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
    logger.info(`Canceling query for ${queryContext.searchString}`);

    if (this._suggestionsController) {
      this._suggestionsController.stop();
      this._suggestionsController = null;
    }

    this.queries.delete(queryContext);
  }

  /**
   * Returns the number of form history entries we should fetch from the
   * suggestions controller for a given query context.
   *
   * @param {object} queryContext The query context object
   * @returns {number} The number of form history results we should fetch.
   */
  _getFormHistoryCount(queryContext) {
    if (!this._allowSuggestions(queryContext)) {
      return 0;
    }

    let count = UrlbarPrefs.get("maxHistoricalSearchSuggestions");
    if (!count) {
      return 0;
    }

    // If there's a form history entry that equals the search string, the search
    // suggestions controller will include it, and we'll make a result for it.
    // If the heuristic result ends up being a search result, the muxer will
    // exclude the form history result since it dupes the heuristic, and the
    // final list of results would be left with `count` - 1 form history results
    // instead of `count`.  Therefore we request `count` + 1 entries.  The muxer
    // will dedupe and limit the final form history count as appropriate.
    return count + 1;
  }

  async _fetchSearchSuggestions(queryContext, engine, searchString, alias) {
    if (!engine || !searchString) {
      return null;
    }

    this._suggestionsController = new SearchSuggestionController();
    this._suggestionsController.formHistoryParam = queryContext.formHistoryName;
    this._suggestionsController.maxLocalResults = this._getFormHistoryCount(
      queryContext
    );

    let allowRemote = this._allowRemoteSuggestions(queryContext);

    // Request maxResults + 1 remote suggestions for the same reason we request
    // maxHistoricalSearchSuggestions + 1 form history entries; see
    // _getFormHistoryCount.  We allow for the possibility that the engine may
    // return a suggestion that's the same as the search string.
    this._suggestionsController.maxRemoteResults = allowRemote
      ? queryContext.maxResults + 1
      : 0;

    this._suggestionsFetchCompletePromise = this._suggestionsController.fetch(
      searchString,
      queryContext.isPrivate,
      engine,
      queryContext.userContextId
    );

    // See `SearchSuggestionsController.fetch` documentation for a description
    // of `fetchData`.
    let fetchData = await this._suggestionsFetchCompletePromise;
    // The fetch was canceled.
    if (!fetchData) {
      return null;
    }

    let results = [];

    for (let entry of fetchData.local) {
      results.push(
        new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.SEARCH,
          UrlbarUtils.RESULT_SOURCE.HISTORY,
          ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            engine: engine.name,
            suggestion: [entry.value, UrlbarUtils.HIGHLIGHT.SUGGESTED],
            lowerCaseSuggestion: entry.value.toLocaleLowerCase(),
          })
        )
      );
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
      logger,
    });

    for (let entry of fetchData.remote) {
      if (looksLikeUrl(entry.value)) {
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
              tailOffsetIndex: entry.tailOffsetIndex,
              keyword: [alias ? alias : undefined, UrlbarUtils.HIGHLIGHT.TYPED],
              query: [searchString.trim(), UrlbarUtils.HIGHLIGHT.NONE],
              isSearchHistory: false,
              icon: [engine.iconURI && !entry.value ? engine.iconURI.spec : ""],
              keywordOffer: UrlbarUtils.KEYWORD_OFFER.NONE,
            })
          )
        );
      } catch (err) {
        Cu.reportError(err);
        continue;
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
   * @returns {boolean} aliasEngine.isTokenAlias
   *
   */
  async _maybeGetAlias(queryContext) {
    if (
      queryContext.restrictSource &&
      queryContext.restrictSource == UrlbarUtils.RESULT_SOURCE.SEARCH &&
      queryContext.engineName &&
      !queryContext.searchString.startsWith("@")
    ) {
      // If an engineName was passed in from the queryContext in restrict mode,
      // we'll set our engine in startQuery based on engineName.
      return null;
    }

    let possibleAlias = queryContext.tokens[0]?.value.trim();
    // The "@" character on its own is handled by UnifiedComplete and returns a
    // list of every available token alias.
    if (!possibleAlias || possibleAlias == "@") {
      return null;
    }

    // Check if the user entered an engine alias directly.
    let engineMatch = await PlacesSearchAutocompleteProvider.engineForAlias(
      possibleAlias
    );
    if (engineMatch) {
      return {
        engine: engineMatch,
        alias: possibleAlias,
        query: substringAfter(queryContext.searchString, possibleAlias).trim(),
        isTokenAlias: possibleAlias.startsWith("@"),
      };
    }

    // Check if the user is matching a token alias.
    let engines = await PlacesSearchAutocompleteProvider.tokenAliasEngines();
    if (!engines || !engines.length) {
      return null;
    }

    for (let { engine, tokenAliases } of engines) {
      if (tokenAliases.includes(possibleAlias)) {
        return {
          engine,
          alias: possibleAlias,
          query: substringAfter(
            queryContext.searchString,
            possibleAlias
          ).trim(),
          isTokenAlias: true,
        };
      }
    }
    return null;
  }
}

var UrlbarProviderSearchSuggestions = new ProviderSearchSuggestions();
