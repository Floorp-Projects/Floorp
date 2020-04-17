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
  Services: "resource://gre/modules/Services.jsm",
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
    if (
      !UrlbarPrefs.get("browser.search.suggest.enabled") ||
      !UrlbarPrefs.get("suggest.searches")
    ) {
      return false;
    }

    if (!queryContext.allowSearchSuggestions) {
      return false;
    }

    if (
      queryContext.isPrivate &&
      !UrlbarPrefs.get("browser.search.suggest.enabled.private")
    ) {
      return false;
    }

    // This condition is met if the user entered a restriction token other than
    // the token for search.
    if (!queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.SEARCH)) {
      return false;
    }

    if (
      queryContext.restrictSource &&
      queryContext.restrictSource != UrlbarUtils.RESULT_SOURCE.SEARCH
    ) {
      return false;
    }

    // If the user is just adding on to a query that previously didn't return
    // many results, we are unlikely to get any more results.
    if (
      !!this._lastLowResultsSearchSuggestion &&
      queryContext.searchString.length >
        this._lastLowResultsSearchSuggestion.length &&
      queryContext.searchString.startsWith(this._lastLowResultsSearchSuggestion)
    ) {
      return false;
    }

    // Never prohibit suggestions when the user used a search engine token
    // alias.  We want "@engine query" to return suggestions from the engine.
    // We'll return early from startQuery if the query doesn't match an alias.
    if (queryContext.searchString.startsWith("@")) {
      return true;
    }

    // We're unlikely to get useful suggestions for single-character queries.
    if (queryContext.searchString.length < 2) {
      return false;
    }

    // The first token may be a whitelisted host.
    if (
      queryContext.tokens.length == 1 &&
      queryContext.tokens[0].type == UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN &&
      Services.uriFixup.isDomainWhitelisted(queryContext.tokens[0].value, -1)
    ) {
      return false;
    }

    // Disallow fetching search suggestions for strings looking like URLs, or
    // non-alphanumeric origins, to avoid disclosing information about networks
    // or passwords.
    return !queryContext.tokens.some(t => {
      return (
        t.type == UrlbarTokenizer.TYPE.POSSIBLE_URL ||
        (t.type == UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN &&
          !/^[a-z0-9-]+$/i.test(t.value))
      );
    });
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
      // UnifiedComplete matches queries starting with "@" to token alias
      // engines. If the string starts with "@", but an alias engine is not yet
      // matched, then UnifiedComplete might still be returning token alias
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
    let results = await this._matchSearchSuggestions(
      queryContext,
      engine,
      query,
      alias
    );

    if (!this.queries.has(queryContext)) {
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

    if (this._suggestionsFetch) {
      this._suggestionsFetch.stop();
      this._suggestionsFetch = null;
    }

    this.queries.delete(queryContext);
  }

  async _matchSearchSuggestions(queryContext, engine, searchString, alias) {
    this._suggestionsFetch = PlacesSearchAutocompleteProvider.newSuggestionsFetch(
      engine,
      searchString,
      queryContext.isPrivate,
      UrlbarPrefs.get("maxHistoricalSearchSuggestions"),
      queryContext.maxResults -
        UrlbarPrefs.get("maxHistoricalSearchSuggestions"),
      queryContext.userContextId
    );
    await this._suggestionsFetch.fetchCompletePromise;
    try {
      // The fetch has been canceled already.
      if (!this._suggestionsFetch) {
        return null;
      }
      // If we don't return many results, then keep track of the query. If the
      // user just adds on to the query, we won't fetch more suggestions since
      // we are unlikely to get any.
      if (
        this._suggestionsFetch.resultsCount >= 0 &&
        this._suggestionsFetch.resultsCount < 2
      ) {
        this._lastLowResultsSearchSuggestion = searchString;
      }
      let results = [];
      let result;
      while ((result = this._suggestionsFetch.consume())) {
        if (
          !result ||
          result.suggestion == searchString ||
          looksLikeUrl(result.suggestion)
        ) {
          continue;
        }

        results.push(
          new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.SEARCH,
            UrlbarUtils.RESULT_SOURCE.SEARCH,
            ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
              engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
              suggestion: [result.suggestion, UrlbarUtils.HIGHLIGHT.SUGGESTED],
              keyword: [alias ? alias : undefined, UrlbarUtils.HIGHLIGHT.TYPED],
              query: [searchString.trim(), UrlbarUtils.HIGHLIGHT.TYPED],
              icon: [
                engine.iconURI && !result.suggestion ? engine.iconURI.spec : "",
              ],
              keywordOffer: UrlbarUtils.KEYWORD_OFFER.NONE,
            })
          )
        );
      }

      return results;
    } catch (err) {
      Cu.reportError(err);
      return null;
    }
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
