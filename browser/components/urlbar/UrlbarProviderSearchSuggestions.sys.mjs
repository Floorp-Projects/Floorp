/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that offers search engine suggestions.
 */

import {
  SkippableTimer,
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
});

const RESULT_MENU_COMMANDS = {
  TRENDING_BLOCK: "trendingblock",
  TRENDING_HELP: "help",
};

const TRENDING_HELP_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "google-trending-searches-on-awesomebar";

/**
 * Returns whether the passed in string looks like a url.
 *
 * @param {string} str
 *   The string to check.
 * @param {boolean} [ignoreAlphanumericHosts]
 *   If true, don't consider a string with an alphanumeric host to be a URL.
 * @returns {boolean}
 *   True if the query looks like a URL.
 */
function looksLikeUrl(str, ignoreAlphanumericHosts = false) {
  // Single word including special chars.
  return (
    !lazy.UrlbarTokenizer.REGEXP_SPACES.test(str) &&
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
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "SearchSuggestions";
  }

  /**
   * Returns the type of this provider.
   *
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.NETWORK;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
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
    // search or showing trending suggestions.
    if (
      !queryContext.trimmedSearchString &&
      !this._isTokenOrRestrictionPresent(queryContext) &&
      !this.#shouldFetchTrending(queryContext)
    ) {
      return false;
    }

    if (!this._allowSuggestions(queryContext)) {
      return false;
    }

    let wantsLocalSuggestions =
      lazy.UrlbarPrefs.get("maxHistoricalSearchSuggestions") &&
      (queryContext.trimmedSearchString ||
        lazy.UrlbarPrefs.get("update2.emptySearchBehavior") != 0);

    return wantsLocalSuggestions || this._allowRemoteSuggestions(queryContext);
  }

  /**
   * Returns whether the user typed a token alias or restriction token, or is in
   * search mode. We use this value to override the pref to disable search
   * suggestions in the Urlbar.
   *
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
        t => t.type == lazy.UrlbarTokenizer.TYPE.RESTRICT_SEARCH
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
      // If the user typed a restriction token or token alias, we ignore the
      // pref to disable suggestions in the Urlbar.
      (!lazy.UrlbarPrefs.get("suggest.searches") &&
        !this._isTokenOrRestrictionPresent(queryContext)) ||
      !lazy.UrlbarPrefs.get("browser.search.suggest.enabled") ||
      (queryContext.isPrivate &&
        !lazy.UrlbarPrefs.get("browser.search.suggest.enabled.private"))
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
    // This is checked by `queryContext.allowRemoteResults` below, but we can
    // short-circuit that call with the `_isTokenOrRestrictionPresent` block
    // before that. Make sure we don't allow remote suggestions if this is set.
    if (queryContext.prohibitRemoteResults) {
      return false;
    }

    // Allow remote suggestions if trending suggestions are enabled.
    if (this.#shouldFetchTrending(queryContext)) {
      return true;
    }

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

    return queryContext.allowRemoteResults(
      searchString,
      lazy.UrlbarPrefs.get("trending.featureGate")
    );
  }

  /**
   * Starts querying.
   *
   * @param {object} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
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
      lazy.UrlbarTokenizer.isRestrictionToken(queryContext.tokens[0]) &&
      (queryContext.tokens.length > 1 ||
        queryContext.tokens[0].type ==
          lazy.UrlbarTokenizer.TYPE.RESTRICT_SEARCH)
    ) {
      leadingRestrictionToken = queryContext.tokens[0].value;
    }

    // Strip a leading search restriction char, because we prepend it to text
    // when the search shortcut is used and it's not user typed. Don't strip
    // other restriction chars, so that it's possible to search for things
    // including one of those (e.g. "c#").
    if (leadingRestrictionToken === lazy.UrlbarTokenizer.RESTRICT.SEARCH) {
      query = UrlbarUtils.substringAfter(query, leadingRestrictionToken).trim();
    }

    // Find our search engine. It may have already been set with an alias.
    let engine;
    if (aliasEngine) {
      engine = aliasEngine.engine;
    } else if (queryContext.searchMode?.engineName) {
      engine = lazy.UrlbarSearchUtils.getEngineByName(
        queryContext.searchMode.engineName
      );
    } else {
      engine = lazy.UrlbarSearchUtils.getDefaultEngine(queryContext.isPrivate);
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
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   */
  getPriority(queryContext) {
    if (this.#shouldFetchTrending(queryContext)) {
      return lazy.UrlbarProviderTopSites.PRIORITY;
    }
    return 0;
  }

  /**
   * Cancels a running query.
   *
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    if (this._suggestionsController) {
      this._suggestionsController.stop();
      this._suggestionsController = null;
    }
  }

  /**
   * Returns the menu commands to be shown for trending results.
   *
   * @param {UrlbarResult} result
   *   The result to get menu comands for.
   *
   * @returns {Array} The commands to be shown.
   */
  getResultCommands(result) {
    if (result.payload.trending) {
      return [
        {
          name: RESULT_MENU_COMMANDS.TRENDING_BLOCK,
          l10n: { id: "urlbar-result-menu-trending-dont-show" },
        },
        {
          name: "separator",
        },
        {
          name: RESULT_MENU_COMMANDS.TRENDING_HELP,
          l10n: { id: "urlbar-result-menu-trending-why" },
        },
      ];
    }
    return undefined;
  }

  onEngagement(state, queryContext, details, controller) {
    let { result } = details;
    if (result?.providerName != this.name) {
      return;
    }

    if (details.selType == "dismiss" && queryContext.formHistoryName) {
      lazy.FormHistory.update({
        op: "remove",
        fieldname: queryContext.formHistoryName,
        value: result.payload.suggestion,
      }).catch(error =>
        console.error(`Removing form history failed: ${error}`)
      );
      controller.removeResult(result);
      return;
    }

    switch (details.selType) {
      case RESULT_MENU_COMMANDS.TRENDING_HELP:
        // Handled by UrlbarInput
        break;
      case RESULT_MENU_COMMANDS.TRENDING_BLOCK:
        lazy.UrlbarPrefs.set("suggest.trending", false);
        this.#recordTrendingBlockedTelemetry(details.selType);
        this.#replaceTrendingResultWithAcknowledgement(controller);
        break;
    }
  }

  async _fetchSearchSuggestions(queryContext, engine, searchString, alias) {
    if (!engine) {
      return null;
    }

    this._suggestionsController = new lazy.SearchSuggestionController(
      queryContext.formHistoryName
    );

    // If there's a form history entry that equals the search string, the search
    // suggestions controller will include it, and we'll make a result for it.
    // If the heuristic result ends up being a search result, the muxer will
    // discard the form history result since it dupes the heuristic, and the
    // final list of results would be left with `count` - 1 form history results
    // instead of `count`.  Therefore we request `count` + 1 entries.  The muxer
    // will dedupe and limit the final form history count as appropriate.
    this._suggestionsController.maxLocalResults = queryContext.maxResults + 1;

    // Request maxResults + 1 remote suggestions for the same reason we request
    // maxResults + 1 form history entries.
    let allowRemote = this._allowRemoteSuggestions(queryContext, searchString);
    this._suggestionsController.maxRemoteResults = allowRemote
      ? queryContext.maxResults + 1
      : 0;

    if (allowRemote && this.#shouldFetchTrending(queryContext)) {
      if (
        queryContext.searchMode &&
        lazy.UrlbarPrefs.get("trending.maxResultsSearchMode") != -1
      ) {
        this._suggestionsController.maxRemoteResults = lazy.UrlbarPrefs.get(
          "trending.maxResultsSearchMode"
        );
      } else if (
        !queryContext.searchMode &&
        lazy.UrlbarPrefs.get("trending.maxResultsNoSearchMode") != -1
      ) {
        this._suggestionsController.maxRemoteResults = lazy.UrlbarPrefs.get(
          "trending.maxResultsNoSearchMode"
        );
      }
    }

    this._suggestionsFetchCompletePromise = this._suggestionsController.fetch(
      searchString,
      queryContext.isPrivate,
      engine,
      queryContext.userContextId,
      this._isTokenOrRestrictionPresent(queryContext),
      false,
      this.#shouldFetchTrending(queryContext)
    );

    // See `SearchSuggestionsController.fetch` documentation for a description
    // of `fetchData`.
    let fetchData = await this._suggestionsFetchCompletePromise;
    // The fetch was canceled.
    if (!fetchData) {
      return null;
    }

    let results = [];

    // maxHistoricalSearchSuggestions used to determine the initial number of
    // form history results, with the special case where zero means to never
    // show form history at all.  With the introduction of flexed result
    // groups, we now use it only as a boolean: Zero means don't show form
    // history at all (as before), non-zero means show it.
    if (lazy.UrlbarPrefs.get("maxHistoricalSearchSuggestions")) {
      for (let entry of fetchData.local) {
        results.push(makeFormHistoryResult(queryContext, engine, entry));
      }
    }

    // If we don't return many results, then keep track of the query. If the
    // user just adds on to the query, we won't fetch more suggestions if the
    // query is very long since we are unlikely to get any.
    if (
      allowRemote &&
      !fetchData.remote.length &&
      searchString.length > lazy.UrlbarPrefs.get("maxCharsForSearchSuggestions")
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
      if (looksLikeUrl(entry.value)) {
        continue;
      }

      let tail = entry.tail;
      let tailPrefix = entry.matchPrefix;

      // Skip tail suggestions if the pref is disabled.
      if (tail && !lazy.UrlbarPrefs.get("richSuggestions.tail")) {
        continue;
      }

      if (!tail) {
        await tailTimer.fire().catch(ex => this.logger.error(ex));
      }

      try {
        let payload = {
          engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
          suggestion: [entry.value, UrlbarUtils.HIGHLIGHT.SUGGESTED],
          lowerCaseSuggestion: entry.value.toLocaleLowerCase(),
          tailPrefix,
          tail: [tail, UrlbarUtils.HIGHLIGHT.SUGGESTED],
          tailOffsetIndex: tail ? entry.tailOffsetIndex : undefined,
          keyword: [alias ? alias : undefined, UrlbarUtils.HIGHLIGHT.TYPED],
          trending: entry.trending,
          description: entry.description || undefined,
          query: [searchString.trim(), UrlbarUtils.HIGHLIGHT.NONE],
          icon: !entry.value ? engine.iconURI?.spec : entry.icon,
        };

        if (entry.trending) {
          payload.helpUrl = TRENDING_HELP_URL;
        }

        results.push(
          Object.assign(
            new lazy.UrlbarResult(
              UrlbarUtils.RESULT_TYPE.SEARCH,
              UrlbarUtils.RESULT_SOURCE.SEARCH,
              ...lazy.UrlbarResult.payloadAndSimpleHighlights(
                queryContext.tokens,
                payload
              )
            ),
            { isRichSuggestion: !!entry.icon }
          )
        );
      } catch (err) {
        this.logger.error(err);
        continue;
      }
    }

    await tailTimer.promise;
    return results;
  }

  /**
   * @typedef {object} EngineAlias
   *
   * @property {nsISearchEngine} engine
   *   The search engine
   * @property {string} alias
   *   The search engine's alias
   * @property {string} query
   *   The remainder of the search engine string after the alias
   */

  /**
   * Searches for an engine alias given the queryContext.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context object.
   * @returns {EngineAlias?} aliasEngine
   *   A representation of the aliased engine. Null if there's no match.
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
    if (!lazy.UrlbarTokenizer.REGEXP_SPACES_START.test(query)) {
      return null;
    }

    // Check if the user entered an engine alias directly.
    let engineMatch = await lazy.UrlbarSearchUtils.engineForAlias(
      possibleAlias
    );
    if (engineMatch) {
      return {
        engine: engineMatch,
        alias: possibleAlias,
        query: query.trim(),
      };
    }

    return null;
  }

  /**
   * Whether we should show trending suggestions. These are shown when the
   * user enters a specific engines searchMode when enabled, the
   * seperate `requireSearchMode` pref controls whether they are visible
   * when the urlbar is first opened without any search mode.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context object.
   * @returns {boolean}
   *   Whether we should fetch trending results.
   */
  #shouldFetchTrending(queryContext) {
    return !!(
      queryContext.searchString == "" &&
      lazy.UrlbarPrefs.get("trending.featureGate") &&
      lazy.UrlbarPrefs.get("suggest.trending") &&
      (queryContext.searchMode ||
        !lazy.UrlbarPrefs.get("trending.requireSearchMode"))
    );
  }

  /*
   * Send telemetry to indicating trending results have been hidden.
   */
  #recordTrendingBlockedTelemetry() {
    Services.telemetry.scalarAdd("urlbar.trending.block", 1);
  }

  /*
   * Remove all the trending results and show an acknowledgement that the
   * trending suggestions have been turned off.
   */
  #replaceTrendingResultWithAcknowledgement(controller) {
    let resultsToRemove = controller.view.visibleResults.filter(
      result => result.payload.trending
    );
    if (resultsToRemove.length) {
      // Show an acknowledgement tip for the first result.
      resultsToRemove[0].acknowledgeDismissalL10n = {
        id: "urlbar-trending-dismissal-acknowledgment",
      };
    }
    // Remove results in reverse order so the acknowledgment tip isn't removed.
    resultsToRemove.reverse();
    resultsToRemove.forEach(result => controller.removeResult(result));
  }
}

function makeFormHistoryResult(queryContext, engine, entry) {
  return new lazy.UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
      engine: engine.name,
      suggestion: [entry.value, UrlbarUtils.HIGHLIGHT.SUGGESTED],
      lowerCaseSuggestion: entry.value.toLocaleLowerCase(),
    })
  );
}

export var UrlbarProviderSearchSuggestions = new ProviderSearchSuggestions();
