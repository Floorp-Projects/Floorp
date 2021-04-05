/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a component used to register search providers and manage
 * the connection between such providers and a UrlbarController.
 */

var EXPORTED_SYMBOLS = ["UrlbarProvidersManager"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  SkippableTimer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  UrlbarUtils.getLogger({ prefix: "ProvidersManager" })
);

// List of available local providers, each is implemented in its own jsm module
// and will track different queries internally by queryContext.
var localProviderModules = {
  UrlbarProviderUnifiedComplete:
    "resource:///modules/UrlbarProviderUnifiedComplete.jsm",
  UrlbarProviderAutofill: "resource:///modules/UrlbarProviderAutofill.jsm",
  UrlbarProviderCalculator: "resource:///modules/UrlbarProviderCalculator.jsm",
  UrlbarProviderHeuristicFallback:
    "resource:///modules/UrlbarProviderHeuristicFallback.jsm",
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.jsm",
  UrlbarProviderOmnibox: "resource:///modules/UrlbarProviderOmnibox.jsm",
  UrlbarProviderPrivateSearch:
    "resource:///modules/UrlbarProviderPrivateSearch.jsm",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  UrlbarProviderSearchTips: "resource:///modules/UrlbarProviderSearchTips.jsm",
  UrlbarProviderSearchSuggestions:
    "resource:///modules/UrlbarProviderSearchSuggestions.jsm",
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.jsm",
  UrlbarProviderTokenAliasEngines:
    "resource:///modules/UrlbarProviderTokenAliasEngines.jsm",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.jsm",
};

// List of available local muxers, each is implemented in its own jsm module.
var localMuxerModules = {
  UrlbarMuxerUnifiedComplete:
    "resource:///modules/UrlbarMuxerUnifiedComplete.jsm",
};

// To improve dataflow and reduce UI work, when a result is added by a
// non-heuristic provider, we notify it to the controller after a delay, so
// that we can chunk results coming in that timeframe into a single call.
const CHUNK_RESULTS_DELAY_MS = 16;

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
      let { [symbol]: provider } = ChromeUtils.import(module, {});
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
      let { [symbol]: muxer } = ChromeUtils.import(module, {});
      this.registerMuxer(muxer);
    }
  }

  /**
   * Registers a provider object with the manager.
   * @param {object} provider
   */
  registerProvider(provider) {
    if (!provider || !(provider instanceof UrlbarProvider)) {
      throw new Error(`Trying to register an invalid provider`);
    }
    if (!Object.values(UrlbarUtils.PROVIDER_TYPE).includes(provider.type)) {
      throw new Error(`Unknown provider type ${provider.type}`);
    }
    logger.info(`Registering provider ${provider.name}`);
    let index = -1;
    if (provider.type == UrlbarUtils.PROVIDER_TYPE.HEURISTIC) {
      // Keep heuristic providers in order at the front of the array.  Find the
      // first non-heuristic provider and insert the new provider there.
      index = this.providers.findIndex(
        p => p.type != UrlbarUtils.PROVIDER_TYPE.HEURISTIC
      );
    }
    if (index < 0) {
      index = this.providers.length;
    }
    this.providers.splice(index, 0, provider);
  }

  /**
   * Unregisters a previously registered provider object.
   * @param {object} provider
   */
  unregisterProvider(provider) {
    logger.info(`Unregistering provider ${provider.name}`);
    let index = this.providers.findIndex(p => p.name == provider.name);
    if (index != -1) {
      this.providers.splice(index, 1);
    }
  }

  /**
   * Returns the provider with the given name.
   * @param {string} name The provider name.
   * @returns {UrlbarProvider} The provider.
   */
  getProvider(name) {
    return this.providers.find(p => p.name == name);
  }

  /**
   * Registers a muxer object with the manager.
   * @param {object} muxer a UrlbarMuxer object
   */
  registerMuxer(muxer) {
    if (!muxer || !(muxer instanceof UrlbarMuxer)) {
      throw new Error(`Trying to register an invalid muxer`);
    }
    logger.info(`Registering muxer ${muxer.name}`);
    this.muxers.set(muxer.name, muxer);
  }

  /**
   * Unregisters a previously registered muxer object.
   * @param {object} muxer a UrlbarMuxer object or name.
   */
  unregisterMuxer(muxer) {
    let muxerName = typeof muxer == "string" ? muxer : muxer.name;
    logger.info(`Unregistering muxer ${muxerName}`);
    this.muxers.delete(muxerName);
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {object} [controller] a UrlbarController instance
   */
  async startQuery(queryContext, controller = null) {
    logger.info(`Query start ${queryContext.searchString}`);

    // Define the muxer to use.
    let muxerName = queryContext.muxer || DEFAULT_MUXER;
    logger.info(`Using muxer ${muxerName}`);
    let muxer = this.muxers.get(muxerName);
    if (!muxer) {
      throw new Error(`Muxer with name ${muxerName} not found`);
    }

    // If the queryContext specifies a list of providers to use, filter on it,
    // otherwise just pass the full list of providers.
    let providers = queryContext.providers
      ? this.providers.filter(p => queryContext.providers.includes(p.name))
      : this.providers;

    // Apply tokenization.
    UrlbarTokenizer.tokenize(queryContext);

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
      if (UrlbarTokenizer.SEARCH_MODE_RESTRICT.has(restrictToken.value)) {
        queryContext.restrictSource = queryContext.sources[0];
      }
    }
    logger.debug(`Context sources ${queryContext.sources}`);

    let query = new Query(queryContext, controller, muxer, providers);
    this.queries.set(queryContext, query);

    // The muxer and many providers depend on the search service and our search
    // utils.  Make sure they're initialized now (via UrlbarSearchUtils) so that
    // all query-related urlbar modules don't need to do it.
    await UrlbarSearchUtils.init();
    if (query.canceled) {
      return;
    }

    // Update the behavior of extension providers.
    let updateBehaviorPromises = [];
    for (let provider of this.providers) {
      if (
        provider.type == UrlbarUtils.PROVIDER_TYPE.EXTENSION &&
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
   * @param {object} queryContext
   */
  cancelQuery(queryContext) {
    logger.info(`Query cancel "${queryContext.searchString}"`);
    let query = this.queries.get(queryContext);
    if (!query) {
      throw new Error("Couldn't find a matching query for the given context");
    }
    query.cancel();
    if (!this.interruptLevel) {
      try {
        let db = PlacesUtils.promiseLargeCacheDBConnection();
        db.interrupt();
      } catch (ex) {}
    }
    this.queries.delete(queryContext);
  }

  /**
   * A provider can use this util when it needs to run a SQL query that can't
   * be interrupted. Otherwise, when a query is canceled any running SQL query
   * is interrupted abruptly.
   * @param {function} taskFn a Task to execute in the critical section.
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
   * @param {boolean} isPrivate
   *   True if the engagement is in a private context.
   * @param {string} state
   *   The state of the engagement, one of: start, engagement, abandonment,
   *   discard
   * @param {UrlbarQueryContext} queryContext
   *   The engagement's query context, if available.
   * @param {object} details
   *   An object that describes the search string and the picked result, if any.
   */
  notifyEngagementChange(isPrivate, state, queryContext, details) {
    for (let provider of this.providers) {
      provider.tryMethod(
        "onEngagement",
        isPrivate,
        state,
        queryContext,
        details
      );
    }
  }
}

var UrlbarProvidersManager = new ProvidersManager();

/**
 * Tracks a query status.
 * Multiple queries can potentially be executed at the same time by different
 * controllers. Each query has to track its own status and delays separately,
 * to avoid conflicting with other ones.
 */
class Query {
  /**
   * Initializes the query object.
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
        Promise.resolve(provider.tryMethod("isActive", this.context))
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
          .catch(Cu.reportError)
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
      provider.logger.info(`Starting query for "${this.context.searchString}"`);
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
      if (provider.type == UrlbarUtils.PROVIDER_TYPE.HEURISTIC) {
        this.context.pendingHeuristicProviders.add(provider.name);
        queryPromises.push(startQuery(provider));
        continue;
      }
      if (!this._sleepTimer) {
        // Tracks the delay timer. We will fire (in this specific case, cancel
        // would do the same, since the callback is empty) the timer when the
        // search is canceled, unblocking start().
        this._sleepTimer = new SkippableTimer({
          name: "Query provider timer",
          time: UrlbarPrefs.get("delay"),
          logger: provider.logger,
        });
      }
      queryPromises.push(
        this._sleepTimer.promise.then(() =>
          this.canceled ? undefined : startQuery(provider)
        )
      );
    }

    logger.info(`Queried ${queryPromises.length} providers`);
    await Promise.all(queryPromises);

    // All the providers are done returning results, so we can stop chunking.
    if (!this.canceled) {
      if (this._heuristicProviderTimer) {
        await this._heuristicProviderTimer.fire();
      }
      if (this._chunkTimer) {
        await this._chunkTimer.fire();
      }
    }

    // Break cycles with the controller to avoid leaks.
    this.controller = null;
  }

  /**
   * Cancels this query.
   * @note Invoking cancel multiple times is a no-op.
   */
  cancel() {
    if (this.canceled) {
      return;
    }
    this.canceled = true;
    this.context.deferUserSelectionProviders.clear();
    for (let provider of this.providers) {
      provider.logger.info(
        `Canceling query for "${this.context.searchString}"`
      );
      // Mark the instance as no more valid, see start() for details.
      provider.queryInstance = null;
      provider.tryMethod("cancelQuery", this.context);
    }
    if (this._heuristicProviderTimer) {
      this._heuristicProviderTimer.cancel().catch(Cu.reportError);
    }
    if (this._chunkTimer) {
      this._chunkTimer.cancel().catch(Cu.reportError);
    }
    if (this._sleepTimer) {
      this._sleepTimer.fire().catch(Cu.reportError);
    }
  }

  /**
   * Adds a result returned from a provider to the results set.
   * @param {object} provider
   * @param {object} result
   */
  add(provider, result) {
    if (!(provider instanceof UrlbarProvider)) {
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
      (result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        result.source != UrlbarUtils.RESULT_SOURCE.HISTORY ||
        !this.acceptableSources.includes(UrlbarUtils.RESULT_SOURCE.SEARCH))
    ) {
      return;
    }

    // Filter out javascript results for safety. The provider is supposed to do
    // it, but we don't want to risk leaking these out.
    if (
      result.type != UrlbarUtils.RESULT_TYPE.KEYWORD &&
      result.payload.url &&
      result.payload.url.startsWith("javascript:") &&
      !this.context.searchString.startsWith("javascript:") &&
      UrlbarPrefs.get("filter.javascript")
    ) {
      return;
    }

    result.providerName = provider.name;
    result.providerType = provider.type;
    this.context.results.push(result);

    this._notifyResultsFromProvider(provider);
  }

  _notifyResultsFromProvider(provider) {
    // We create two chunking timers: one for heuristic results, and one for
    // other results. We expect heuristic providers to return their heuristic
    // results before other results/providers in most cases. When all heuristic
    // providers have returned some results, we fire the heuristic timer early.
    // If the timer fires first, we stop waiting on the remaining heuristic
    // providers.
    // Both timers are used to reduce UI flicker.
    if (provider.type == UrlbarUtils.PROVIDER_TYPE.HEURISTIC) {
      if (!this._heuristicProviderTimer) {
        this._heuristicProviderTimer = new SkippableTimer({
          name: "Heuristic provider timer",
          callback: () => this._notifyResults(),
          time: CHUNK_RESULTS_DELAY_MS,
          logger: provider.logger,
        });
      }
    } else if (!this._chunkTimer) {
      this._chunkTimer = new SkippableTimer({
        name: "Query chunk timer",
        callback: () => this._notifyResults(),
        time: CHUNK_RESULTS_DELAY_MS,
        logger: provider.logger,
      });
    }
    // If all active heuristic providers have returned results, we can skip the
    // heuristic results timer and start showing results immediately.
    if (
      this._heuristicProviderTimer &&
      !this.context.pendingHeuristicProviders.size
    ) {
      this._heuristicProviderTimer.fire().catch(Cu.reportError);
    }
  }

  _notifyResults() {
    this.muxer.sort(this.context);

    if (this._heuristicProviderTimer) {
      this._heuristicProviderTimer.cancel().catch(Cu.reportError);
      this._heuristicProviderTimer = null;
    }

    if (this._chunkTimer) {
      this._chunkTimer.cancel().catch(Cu.reportError);
      this._chunkTimer = null;
    }

    // Before the muxer.sort call above, this.context.results should never be
    // empty since this method is called when results are added.  But the muxer
    // may have excluded one or more results from the final sorted list.  For
    // example, it excludes the "Search in a Private Window" result if it's the
    // only result that's been added so far.  We don't want to notify consumers
    // if there are no results since they generally expect at least one result
    // when notified, so bail, but only after nulling out the chunk timer above
    // so that it will be restarted the next time results are added.
    if (!this.context.results.length) {
      return;
    }

    this.context.firstResultChanged = !ObjectUtils.deepEqual(
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
 * @param {UrlbarQueryContext} context The query context to examine
 * @returns {object} The restriction token that was used to set sources, or
 *          undefined if there's no restriction token.
 */
function updateSourcesIfEmpty(context) {
  if (context.sources && context.sources.length) {
    return false;
  }
  let acceptedSources = [];
  // There can be only one restrict token about sources.
  let restrictToken = context.tokens.find(t =>
    [
      UrlbarTokenizer.TYPE.RESTRICT_HISTORY,
      UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK,
      UrlbarTokenizer.TYPE.RESTRICT_TAG,
      UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE,
      UrlbarTokenizer.TYPE.RESTRICT_SEARCH,
    ].includes(t.type)
  );
  let restrictTokenType = restrictToken ? restrictToken.type : undefined;
  for (let source of Object.values(UrlbarUtils.RESULT_SOURCE)) {
    // Skip sources that the context doesn't care about.
    if (context.sources && !context.sources.includes(source)) {
      continue;
    }
    // Check prefs and restriction tokens.
    switch (source) {
      case UrlbarUtils.RESULT_SOURCE.BOOKMARKS:
        if (
          restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK ||
          restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_TAG ||
          (!restrictTokenType && UrlbarPrefs.get("suggest.bookmark"))
        ) {
          acceptedSources.push(source);
        }
        break;
      case UrlbarUtils.RESULT_SOURCE.HISTORY:
        if (
          restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_HISTORY ||
          (!restrictTokenType && UrlbarPrefs.get("suggest.history"))
        ) {
          acceptedSources.push(source);
        }
        break;
      case UrlbarUtils.RESULT_SOURCE.SEARCH:
        if (
          restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_SEARCH ||
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
      case UrlbarUtils.RESULT_SOURCE.TABS:
        if (
          restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE ||
          (!restrictTokenType && UrlbarPrefs.get("suggest.openpage"))
        ) {
          acceptedSources.push(source);
        }
        break;
      case UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK:
        if (!context.isPrivate && !restrictTokenType) {
          acceptedSources.push(source);
        }
        break;
      case UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL:
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
