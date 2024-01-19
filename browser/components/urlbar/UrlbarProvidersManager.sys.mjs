/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a component used to register search providers and manage
 * the connection between such providers and a UrlbarController.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  SkippableTimer: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProvider: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.UrlbarUtils.getLogger({ prefix: "ProvidersManager" })
);

// List of available local providers, each is implemented in its own jsm module
// and will track different queries internally by queryContext.
// When adding new providers please remember to update the list in metrics.yaml.
var localProviderModules = {
  UrlbarProviderAboutPages:
    "resource:///modules/UrlbarProviderAboutPages.sys.mjs",
  UrlbarProviderAliasEngines:
    "resource:///modules/UrlbarProviderAliasEngines.sys.mjs",
  UrlbarProviderAutofill: "resource:///modules/UrlbarProviderAutofill.sys.mjs",
  UrlbarProviderBookmarkKeywords:
    "resource:///modules/UrlbarProviderBookmarkKeywords.sys.mjs",
  UrlbarProviderCalculator:
    "resource:///modules/UrlbarProviderCalculator.sys.mjs",
  UrlbarProviderClipboard:
    "resource:///modules/UrlbarProviderClipboard.sys.mjs",
  UrlbarProviderContextualSearch:
    "resource:///modules/UrlbarProviderContextualSearch.sys.mjs",
  UrlbarProviderHeuristicFallback:
    "resource:///modules/UrlbarProviderHeuristicFallback.sys.mjs",
  UrlbarProviderHistoryUrlHeuristic:
    "resource:///modules/UrlbarProviderHistoryUrlHeuristic.sys.mjs",
  UrlbarProviderInputHistory:
    "resource:///modules/UrlbarProviderInputHistory.sys.mjs",
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.sys.mjs",
  UrlbarProviderOmnibox: "resource:///modules/UrlbarProviderOmnibox.sys.mjs",
  UrlbarProviderPlaces: "resource:///modules/UrlbarProviderPlaces.sys.mjs",
  UrlbarProviderPrivateSearch:
    "resource:///modules/UrlbarProviderPrivateSearch.sys.mjs",
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
  UrlbarProviderQuickSuggestContextualOptIn:
    "resource:///modules/UrlbarProviderQuickSuggestContextualOptIn.sys.mjs",
  UrlbarProviderRecentSearches:
    "resource:///modules/UrlbarProviderRecentSearches.sys.mjs",
  UrlbarProviderRemoteTabs:
    "resource:///modules/UrlbarProviderRemoteTabs.sys.mjs",
  UrlbarProviderSearchTips:
    "resource:///modules/UrlbarProviderSearchTips.sys.mjs",
  UrlbarProviderSearchSuggestions:
    "resource:///modules/UrlbarProviderSearchSuggestions.sys.mjs",
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.sys.mjs",
  UrlbarProviderTokenAliasEngines:
    "resource:///modules/UrlbarProviderTokenAliasEngines.sys.mjs",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.sys.mjs",
  UrlbarProviderUnitConversion:
    "resource:///modules/UrlbarProviderUnitConversion.sys.mjs",
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
};

// List of available local muxers, each is implemented in its own jsm module.
var localMuxerModules = {
  UrlbarMuxerUnifiedComplete:
    "resource:///modules/UrlbarMuxerUnifiedComplete.sys.mjs",
};

const DEFAULT_MUXER = "UnifiedComplete";

/**
 * Class used to create a manager.
 * The manager is responsible to keep a list of providers, instantiate query
 * objects and pass those to the providers.
 */
class ProvidersManager {
  constructor() {
    // Tracks the available providers.  This is a sorted array, with HEURISTIC
    // providers at the front.
    this.providers = [];
    for (let [symbol, module] of Object.entries(localProviderModules)) {
      let { [symbol]: provider } = ChromeUtils.importESModule(module);
      this.registerProvider(provider);
    }
    // Tracks ongoing Query instances by queryContext.
    this.queries = new Map();

    // Interrupt() allows to stop any running SQL query, some provider may be
    // running a query that shouldn't be interrupted, and if so it should
    // bump this through disableInterrupt and enableInterrupt.
    this.interruptLevel = 0;

    // This maps muxer names to muxers.
    this.muxers = new Map();
    for (let [symbol, module] of Object.entries(localMuxerModules)) {
      let { [symbol]: muxer } = ChromeUtils.importESModule(module);
      this.registerMuxer(muxer);
    }

    // These can be set by tests to increase or reduce the chunk delays.
    // See _notifyResultsFromProvider for additional details.
    // To improve dataflow and reduce UI work, when a result is added we may notify
    // it to the controller after a delay, so that we can chunk results in that
    // timeframe into a single call. See _notifyResultsFromProvider for details.
    this.CHUNK_RESULTS_DELAY_MS = 16;
  }

  /**
   * Registers a provider object with the manager.
   *
   * @param {object} provider
   *   The provider object to register.
   */
  registerProvider(provider) {
    if (!provider || !(provider instanceof lazy.UrlbarProvider)) {
      throw new Error(`Trying to register an invalid provider`);
    }
    if (
      !Object.values(lazy.UrlbarUtils.PROVIDER_TYPE).includes(provider.type)
    ) {
      throw new Error(`Unknown provider type ${provider.type}`);
    }
    lazy.logger.info(`Registering provider ${provider.name}`);
    let index = -1;
    if (provider.type == lazy.UrlbarUtils.PROVIDER_TYPE.HEURISTIC) {
      // Keep heuristic providers in order at the front of the array.  Find the
      // first non-heuristic provider and insert the new provider there.
      index = this.providers.findIndex(
        p => p.type != lazy.UrlbarUtils.PROVIDER_TYPE.HEURISTIC
      );
    }
    if (index < 0) {
      index = this.providers.length;
    }
    this.providers.splice(index, 0, provider);
  }

  /**
   * Unregisters a previously registered provider object.
   *
   * @param {object} provider
   *   The provider object to unregister.
   */
  unregisterProvider(provider) {
    lazy.logger.info(`Unregistering provider ${provider.name}`);
    let index = this.providers.findIndex(p => p.name == provider.name);
    if (index != -1) {
      this.providers.splice(index, 1);
    }
  }

  /**
   * Returns the provider with the given name.
   *
   * @param {string} name
   *   The provider name.
   * @returns {UrlbarProvider} The provider.
   */
  getProvider(name) {
    return this.providers.find(p => p.name == name);
  }

  /**
   * Registers a muxer object with the manager.
   *
   * @param {object} muxer
   *   a UrlbarMuxer object
   */
  registerMuxer(muxer) {
    if (!muxer || !(muxer instanceof lazy.UrlbarMuxer)) {
      throw new Error(`Trying to register an invalid muxer`);
    }
    lazy.logger.info(`Registering muxer ${muxer.name}`);
    this.muxers.set(muxer.name, muxer);
  }

  /**
   * Unregisters a previously registered muxer object.
   *
   * @param {object} muxer
   *   a UrlbarMuxer object or name.
   */
  unregisterMuxer(muxer) {
    let muxerName = typeof muxer == "string" ? muxer : muxer.name;
    lazy.logger.info(`Unregistering muxer ${muxerName}`);
    this.muxers.delete(muxerName);
  }

  /**
   * Starts querying.
   *
   * @param {object} queryContext
   *   The query context object
   * @param {object} [controller]
   *   a UrlbarController instance
   */
  async startQuery(queryContext, controller = null) {
    lazy.logger.info(`Query start "${queryContext.searchString}"`);

    // Define the muxer to use.
    let muxerName = queryContext.muxer || DEFAULT_MUXER;
    lazy.logger.debug(`Using muxer ${muxerName}`);
    let muxer = this.muxers.get(muxerName);
    if (!muxer) {
      throw new Error(`Muxer with name ${muxerName} not found`);
    }

    // If the queryContext specifies a list of providers to use, filter on it,
    // otherwise just pass the full list of providers.
    let providers = queryContext.providers
      ? this.providers.filter(p => queryContext.providers.includes(p.name))
      : this.providers;

    queryContext.canceled = false;
    try {
      // The tokenizer needs to synchronously check whether the first token is a
      // keyword, thus here we must ensure the keywords cache is up.
      await lazy.PlacesUtils.keywords.ensureCacheInitialized();
    } catch (ex) {
      lazy.logger.error(
        "Unable to ensure keyword cache is initialization. A keyword may not be \
         detected at the beginning of the search string.",
        ex
      );
    }

    // The query may have been canceled while awaiting for asynchronous work.
    if (queryContext.canceled) {
      return;
    }

    // Apply tokenization.
    lazy.UrlbarTokenizer.tokenize(queryContext);

    // If there's a single source, we are in restriction mode.
    if (queryContext.sources && queryContext.sources.length == 1) {
      queryContext.restrictSource = queryContext.sources[0];
    }
    // Providers can use queryContext.sources to decide whether they want to be
    // invoked or not.
    // The sources may be defined in the context, then the whole search string
    // can be used for searching. Otherwise sources are extracted from prefs and
    // restriction tokens, then restriction tokens must be filtered out of the
    // search string.
    let restrictToken = updateSourcesIfEmpty(queryContext);
    if (restrictToken) {
      queryContext.restrictToken = restrictToken;
      // If the restriction token has an equivalent source, then set it as
      // restrictSource.
      if (lazy.UrlbarTokenizer.SEARCH_MODE_RESTRICT.has(restrictToken.value)) {
        queryContext.restrictSource = queryContext.sources[0];
      }
    }
    lazy.logger.debug(`Context sources ${queryContext.sources}`);

    let query = new Query(queryContext, controller, muxer, providers);
    this.queries.set(queryContext, query);

    // The muxer and many providers depend on the search service and our search
    // utils.  Make sure they're initialized now (via UrlbarSearchUtils) so that
    // all query-related urlbar modules don't need to do it.
    try {
      await lazy.UrlbarSearchUtils.init();
    } catch {
      // We continue anyway, because we want the user to be able to search their
      // history and bookmarks even if search engines are not available.
    }

    if (query.canceled) {
      return;
    }

    // Update the behavior of extension providers.
    let updateBehaviorPromises = [];
    for (let provider of this.providers) {
      if (
        provider.type == lazy.UrlbarUtils.PROVIDER_TYPE.EXTENSION &&
        provider.name != "Omnibox"
      ) {
        updateBehaviorPromises.push(
          provider.tryMethod("updateBehavior", queryContext)
        );
      }
    }
    if (updateBehaviorPromises.length) {
      await Promise.all(updateBehaviorPromises);
      if (query.canceled) {
        return;
      }
    }

    await query.start();
  }

  /**
   * Cancels a running query.
   *
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    lazy.logger.info(`Query cancel "${queryContext.searchString}"`);
    queryContext.canceled = true;

    let query = this.queries.get(queryContext);
    if (!query) {
      // The query object may have not been created yet, if the query was
      // canceled immediately.
      return;
    }
    query.cancel();
    if (!this.interruptLevel) {
      try {
        let db = lazy.PlacesUtils.promiseLargeCacheDBConnection();
        db.interrupt();
      } catch (ex) {}
    }
    this.queries.delete(queryContext);
  }

  /**
   * A provider can use this util when it needs to run a SQL query that can't
   * be interrupted. Otherwise, when a query is canceled any running SQL query
   * is interrupted abruptly.
   *
   * @param {Function} taskFn a Task to execute in the critical section.
   */
  async runInCriticalSection(taskFn) {
    this.interruptLevel++;
    try {
      await taskFn();
    } finally {
      this.interruptLevel--;
    }
  }

  /**
   * Notifies all providers when the user starts and ends an engagement with the
   * urlbar.  For details on parameters, see UrlbarProvider.onEngagement().
   *
   * @param {string} state
   *   The state of the engagement, one of: start, engagement, abandonment,
   *   discard
   * @param {UrlbarQueryContext} queryContext
   *   The engagement's query context, if available.
   * @param {object} details
   *   An object that describes the search string and the picked result, if any.
   * @param {UrlbarController} controller
   *   The controller associated with the engagement
   */
  notifyEngagementChange(state, queryContext, details = {}, controller) {
    for (let provider of this.providers) {
      provider.tryMethod(
        "onEngagement",
        state,
        queryContext,
        details,
        controller
      );
    }
  }
}

export var UrlbarProvidersManager = new ProvidersManager();

/**
 * Tracks a query status.
 * Multiple queries can potentially be executed at the same time by different
 * controllers. Each query has to track its own status and delays separately,
 * to avoid conflicting with other ones.
 */
class Query {
  /**
   * Initializes the query object.
   *
   * @param {object} queryContext
   *        The query context
   * @param {object} controller
   *        The controller to be notified
   * @param {object} muxer
   *        The muxer to sort results
   * @param {Array} providers
   *        Array of all the providers.
   */
  constructor(queryContext, controller, muxer, providers) {
    this.context = queryContext;
    this.context.results = [];
    // Clear any state in the context object, since it could be reused by the
    // caller and we don't want to port previous query state over.
    this.context.pendingHeuristicProviders.clear();
    this.context.deferUserSelectionProviders.clear();
    this.unsortedResults = [];
    this.muxer = muxer;
    this.controller = controller;
    this.providers = providers;
    this.started = false;
    this.canceled = false;

    // This is used as a last safety filter in add(), thus we keep an unmodified
    // copy of it.
    this.acceptableSources = queryContext.sources.slice();
  }

  /**
   * Starts querying.
   */
  async start() {
    if (this.started) {
      throw new Error("This Query has been started already");
    }
    this.started = true;

    // Check which providers should be queried by calling isActive on them.
    let activeProviders = [];
    let activePromises = [];
    let maxPriority = -1;
    for (let provider of this.providers) {
      // This can be used by the provider to check the query is still running
      // after executing async tasks:
      //   let instance = this.queryInstance;
      //   await ...
      //   if (instance != this.queryInstance) {
      //     // Query was canceled or a new one started.
      //     return;
      //   }
      provider.queryInstance = this;
      activePromises.push(
        // Not all isActive implementations are async, so wrap the call in a
        // promise so we can be sure we can call `then` on it.  Note that
        // Promise.resolve returns its arg directly if it's already a promise.
        Promise.resolve(
          provider.tryMethod("isActive", this.context, this.controller)
        )
          .then(isActive => {
            if (isActive && !this.canceled) {
              let priority = provider.tryMethod("getPriority", this.context);
              if (priority >= maxPriority) {
                // The provider's priority is at least as high as the max.
                if (priority > maxPriority) {
                  // The provider's priority is higher than the max.  Remove all
                  // previously added providers, since their priority is
                  // necessarily lower, by setting length to zero.
                  activeProviders.length = 0;
                  maxPriority = priority;
                }
                activeProviders.push(provider);
                if (provider.deferUserSelection) {
                  this.context.deferUserSelectionProviders.add(provider.name);
                }
              }
            }
          })
          .catch(ex => lazy.logger.error(ex))
      );
    }

    // We have to wait for all isActive calls to finish because we want to query
    // only the highest priority active providers as determined by the priority
    // logic above.
    await Promise.all(activePromises);

    if (this.canceled) {
      this.controller = null;
      return;
    }

    // Start querying active providers.
    let startQuery = async provider => {
      provider.logger.debug(
        `Starting query for "${this.context.searchString}"`
      );
      let addedResult = false;
      await provider.tryMethod("startQuery", this.context, (...args) => {
        addedResult = true;
        this.add(...args);
      });
      if (!addedResult) {
        this.context.deferUserSelectionProviders.delete(provider.name);
      }
    };

    let queryPromises = [];
    for (let provider of activeProviders) {
      // Track heuristic providers. later we'll use this Set to wait for them
      // before returning results to the user.
      if (provider.type == lazy.UrlbarUtils.PROVIDER_TYPE.HEURISTIC) {
        this.context.pendingHeuristicProviders.add(provider.name);
        queryPromises.push(
          startQuery(provider).finally(() => {
            this.context.pendingHeuristicProviders.delete(provider.name);
          })
        );
        continue;
      }
      if (!this._sleepTimer) {
        // Tracks the delay timer. We will fire (in this specific case, cancel
        // would do the same, since the callback is empty) the timer when the
        // search is canceled, unblocking start().
        this._sleepTimer = new lazy.SkippableTimer({
          name: "Query provider timer",
          time: lazy.UrlbarPrefs.get("delay"),
          logger: provider.logger,
        });
      }
      queryPromises.push(
        this._sleepTimer.promise.then(() =>
          this.canceled ? undefined : startQuery(provider)
        )
      );
    }

    lazy.logger.info(
      `Queried ${queryPromises.length} providers: ${activeProviders.map(
        p => p.name
      )}`
    );

    // Normally we wait for all the queries, but in case this is canceled we can
    // return earlier.
    let cancelPromise = new Promise(resolve => {
      this._cancelQueries = resolve;
    });
    await Promise.race([Promise.all(queryPromises), cancelPromise]);

    // All the providers are done returning results, so we can stop chunking.
    if (!this.canceled) {
      await this._chunkTimer?.fire();
    }

    // Break cycles with the controller to avoid leaks.
    this.controller = null;
  }

  /**
   * Cancels this query. Note: Invoking cancel multiple times is a no-op.
   */
  cancel() {
    if (this.canceled) {
      return;
    }
    this.canceled = true;
    this.context.deferUserSelectionProviders.clear();
    for (let provider of this.providers) {
      provider.logger.debug(
        `Canceling query for "${this.context.searchString}"`
      );
      // Mark the instance as no more valid, see start() for details.
      provider.queryInstance = null;
      provider.tryMethod("cancelQuery", this.context);
    }
    this._chunkTimer?.cancel().catch(ex => lazy.logger.error(ex));
    this._sleepTimer?.fire().catch(ex => lazy.logger.error(ex));
    this._cancelQueries?.();
  }

  /**
   * Adds a result returned from a provider to the results set.
   *
   * @param {UrlbarProvider} provider The provider that returned the result.
   * @param {object} result The result object.
   */
  add(provider, result) {
    if (!(provider instanceof lazy.UrlbarProvider)) {
      throw new Error("Invalid provider passed to the add callback");
    }

    // When this set is empty, we can display heuristic results early. We remove
    // the provider from the list without checking result.heuristic since
    // heuristic providers don't necessarily have to return heuristic results.
    // We expect a provider with type HEURISTIC will return its heuristic
    // result(s) first.
    this.context.pendingHeuristicProviders.delete(provider.name);

    // Stop returning results as soon as we've been canceled.
    if (this.canceled) {
      return;
    }

    // In search mode, don't allow heuristic results in the following cases
    // since they don't make sense:
    //   * When the search string is empty, or
    //   * In local search mode, except for autofill results
    if (
      result.heuristic &&
      this.context.searchMode &&
      (!this.context.trimmedSearchString ||
        (!this.context.searchMode.engineName && !result.autofill))
    ) {
      return;
    }

    // Check if the result source should be filtered out. Pay attention to the
    // heuristic result though, that is supposed to be added regardless.
    if (
      !this.acceptableSources.includes(result.source) &&
      !result.heuristic &&
      // Treat form history as searches for the purpose of acceptableSources.
      (result.type != lazy.UrlbarUtils.RESULT_TYPE.SEARCH ||
        result.source != lazy.UrlbarUtils.RESULT_SOURCE.HISTORY ||
        !this.acceptableSources.includes(lazy.UrlbarUtils.RESULT_SOURCE.SEARCH))
    ) {
      return;
    }

    // Filter out javascript results for safety. The provider is supposed to do
    // it, but we don't want to risk leaking these out.
    if (
      result.type != lazy.UrlbarUtils.RESULT_TYPE.KEYWORD &&
      result.payload.url &&
      result.payload.url.startsWith("javascript:") &&
      !this.context.searchString.startsWith("javascript:") &&
      lazy.UrlbarPrefs.get("filter.javascript")
    ) {
      return;
    }

    result.providerName = provider.name;
    result.providerType = provider.type;
    this.unsortedResults.push(result);

    this._notifyResultsFromProvider(provider);
  }

  _notifyResultsFromProvider(provider) {
    // We use a timer to reduce UI flicker, by adding results in chunks.
    if (!this._chunkTimer || this._chunkTimer.done) {
      // Either there's no heuristic provider pending at all, or the previous
      // timer is done, but we're still getting results. Start a short timer
      // to chunk remaining results.
      this._chunkTimer = new lazy.SkippableTimer({
        name: "chunking",
        callback: () => this._notifyResults(),
        time: UrlbarProvidersManager.CHUNK_RESULTS_DELAY_MS,
        logger: provider.logger,
      });
    } else if (
      !this.context.pendingHeuristicProviders.size &&
      provider.type == lazy.UrlbarUtils.PROVIDER_TYPE.HEURISTIC
    ) {
      // All the active heuristic providers have returned results, we can skip
      // the heuristic chunk timer and start showing results immediately.
      this._chunkTimer.fire().catch(ex => lazy.logger.error(ex));
    }

    // Otherwise some timer is still ongoing and we'll wait for it.
  }

  _notifyResults() {
    this.muxer.sort(this.context, this.unsortedResults);
    // We don't want to notify consumers if there are no results since they
    // generally expect at least one result when notified, so bail, but only
    // after nulling out the chunk timer above so that it will be restarted
    // the next time results are added.
    if (!this.context.results.length) {
      return;
    }

    this.context.firstResultChanged = !lazy.ObjectUtils.deepEqual(
      this.context.firstResult,
      this.context.results[0]
    );
    this.context.firstResult = this.context.results[0];

    if (this.controller) {
      this.controller.receiveResults(this.context);
    }
  }
}

/**
 * Updates in place the sources for a given UrlbarQueryContext.
 *
 * @param {UrlbarQueryContext} context The query context to examine
 * @returns {object} The restriction token that was used to set sources, or
 *          undefined if there's no restriction token.
 */
function updateSourcesIfEmpty(context) {
  if (context.sources && context.sources.length) {
    return false;
  }
  let acceptedSources = [];
  // There can be only one restrict token per query.
  let restrictToken = context.tokens.find(t =>
    [
      lazy.UrlbarTokenizer.TYPE.RESTRICT_HISTORY,
      lazy.UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK,
      lazy.UrlbarTokenizer.TYPE.RESTRICT_TAG,
      lazy.UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE,
      lazy.UrlbarTokenizer.TYPE.RESTRICT_SEARCH,
      lazy.UrlbarTokenizer.TYPE.RESTRICT_TITLE,
      lazy.UrlbarTokenizer.TYPE.RESTRICT_URL,
      lazy.UrlbarTokenizer.TYPE.RESTRICT_ACTION,
    ].includes(t.type)
  );

  // RESTRICT_TITLE and RESTRICT_URL do not affect query sources.
  let restrictTokenType =
    restrictToken &&
    restrictToken.type != lazy.UrlbarTokenizer.TYPE.RESTRICT_TITLE &&
    restrictToken.type != lazy.UrlbarTokenizer.TYPE.RESTRICT_URL
      ? restrictToken.type
      : undefined;

  for (let source of Object.values(lazy.UrlbarUtils.RESULT_SOURCE)) {
    // Skip sources that the context doesn't care about.
    if (context.sources && !context.sources.includes(source)) {
      continue;
    }
    // Check prefs and restriction tokens.
    switch (source) {
      case lazy.UrlbarUtils.RESULT_SOURCE.BOOKMARKS:
        if (
          restrictTokenType === lazy.UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK ||
          restrictTokenType === lazy.UrlbarTokenizer.TYPE.RESTRICT_TAG ||
          (!restrictTokenType && lazy.UrlbarPrefs.get("suggest.bookmark"))
        ) {
          acceptedSources.push(source);
        }
        break;
      case lazy.UrlbarUtils.RESULT_SOURCE.HISTORY:
        if (
          restrictTokenType === lazy.UrlbarTokenizer.TYPE.RESTRICT_HISTORY ||
          (!restrictTokenType && lazy.UrlbarPrefs.get("suggest.history"))
        ) {
          acceptedSources.push(source);
        }
        break;
      case lazy.UrlbarUtils.RESULT_SOURCE.SEARCH:
        if (
          restrictTokenType === lazy.UrlbarTokenizer.TYPE.RESTRICT_SEARCH ||
          !restrictTokenType
        ) {
          // We didn't check browser.urlbar.suggest.searches here, because it
          // just controls search suggestions. If a search suggestion arrives
          // here, we lost already, because we broke user's privacy by hitting
          // the network. Thus, it's better to leave things go through and
          // notice the bug, rather than hiding it with a filter.
          acceptedSources.push(source);
        }
        break;
      case lazy.UrlbarUtils.RESULT_SOURCE.TABS:
        if (
          restrictTokenType === lazy.UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE ||
          (!restrictTokenType && lazy.UrlbarPrefs.get("suggest.openpage"))
        ) {
          acceptedSources.push(source);
        }
        break;
      case lazy.UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK:
        if (!context.isPrivate && !restrictTokenType) {
          acceptedSources.push(source);
        }
        break;
      case lazy.UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL:
      case lazy.UrlbarUtils.RESULT_SOURCE.ADDON:
      default:
        if (!restrictTokenType) {
          acceptedSources.push(source);
        }
        break;
    }
  }
  context.sources = acceptedSources;
  return restrictToken;
}
