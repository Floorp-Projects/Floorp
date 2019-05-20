/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a component used to register search providers and manage
 * the connection between such providers and a UrlbarController.
 */

var EXPORTED_SYMBOLS = ["UrlbarProvidersManager"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://modules/PlacesUtils.jsm",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.ProvidersManager"));

// List of available local providers, each is implemented in its own jsm module
// and will track different queries internally by queryContext.
var localProviderModules = {
  UrlbarProviderUnifiedComplete: "resource:///modules/UrlbarProviderUnifiedComplete.jsm",
};

// List of available local muxers, each is implemented in its own jsm module.
var localMuxerModules = {
  UrlbarMuxerUnifiedComplete: "resource:///modules/UrlbarMuxerUnifiedComplete.jsm",
};

// To improve dataflow and reduce UI work, when a match is added by a
// non-immediate provider, we notify it to the controller after a delay, so
// that we can chunk matches coming in that timeframe into a single call.
const CHUNK_MATCHES_DELAY_MS = 16;

const DEFAULT_MUXER = "UnifiedComplete";

/**
 * Class used to create a manager.
 * The manager is responsible to keep a list of providers, instantiate query
 * objects and pass those to the providers.
 */
class ProvidersManager {
  constructor() {
    // Tracks the available providers.
    // This is a sorted array, with IMMEDIATE providers at the top.
    this.providers = [];
    for (let [symbol, module] of Object.entries(localProviderModules)) {
      let {[symbol]: provider} = ChromeUtils.import(module, {});
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
      let {[symbol]: muxer} = ChromeUtils.import(module, {});
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
    if (provider.type == UrlbarUtils.PROVIDER_TYPE.IMMEDIATE) {
      this.providers.unshift(provider);
    } else {
      this.providers.push(provider);
    }
  }

  /**
   * Unregisters a previously registered provider object.
   * @param {object} provider
   */
  unregisterProvider(provider) {
    logger.info(`Unregistering provider ${provider.name}`);
    let index = this.providers.indexOf(provider);
    if (index != -1) {
      this.providers.splice(index, 1);
    }
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
   * @param {object} controller a UrlbarController instance
   */
  async startQuery(queryContext, controller) {
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
    let providers = queryContext.providers ?
                      this.providers.filter(p => queryContext.providers.includes(p.name)) :
                      this.providers;

    let query = new Query(queryContext, controller, muxer, providers);
    this.queries.set(queryContext, query);
    await query.start();
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext
   */
  cancelQuery(queryContext) {
    logger.info(`Query cancel ${queryContext.searchString}`);
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
   * @param {object} providers
   *        Map of all the providers by type and name
   */
  constructor(queryContext, controller, muxer, providers) {
    this.context = queryContext;
    this.context.results = [];
    this.muxer = muxer;
    this.controller = controller;
    this.providers = providers;
    this.started = false;
    this.canceled = false;
    this.complete = false;

    // Array of acceptable RESULT_SOURCE values for this query. Providers can
    // use queryContext.acceptableSources to decide whether they want to be
    // invoked or not.
    // This is also used to filter results in add().
    this.acceptableSources = [];
  }

  /**
   * Starts querying.
   */
  async start() {
    if (this.started) {
      throw new Error("This Query has been started already");
    }
    this.started = true;
    UrlbarTokenizer.tokenize(this.context);

    this.acceptableSources = getAcceptableMatchSources(this.context);
    logger.debug(`Acceptable sources ${this.acceptableSources}`);
    // Pass a copy so the provider can't modify our local version.
    this.context.acceptableSources = this.acceptableSources.slice();

    // Check which providers should be queried.
    let providers = this.providers.filter(p => p.isActive(this.context));
    // Check if any of the remaining providers wants to restrict the search.
    let restrictProviders = providers.filter(p => p.isRestricting(this.context));
    if (restrictProviders.length) {
      providers = restrictProviders;
    }

    // Start querying providers.
    let promises = [];
    let delayStarted = false;
    for (let provider of providers) {
      if (this.canceled) {
        break;
      }
      if (provider.type != UrlbarUtils.PROVIDER_TYPE.IMMEDIATE && !delayStarted) {
        delayStarted = true;
        // Tracks the delay timer. We will fire (in this specific case, cancel
        // would do the same, since the callback is empty) the timer when the
        // search is canceled, unblocking start().
        this._sleepTimer = new SkippableTimer(() => {}, UrlbarPrefs.get("delay"));
        await this._sleepTimer.promise;
      }
      promises.push(provider.startQuery(this.context, this.add.bind(this)));
    }

    logger.info(`Queried ${promises.length} providers`);
    if (promises.length) {
      await Promise.all(promises.map(p => p.catch(Cu.reportError)));

      if (this._chunkTimer) {
        // All the providers are done returning results, so we can stop chunking.
        await this._chunkTimer.fire();
      }
    }

    // Nothing should be failing above, since we catch all the promises, thus
    // this is not in a finally for now.
    this.complete = true;
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
    for (let provider of this.providers) {
      provider.cancelQuery(this.context);
    }
    if (this._chunkTimer) {
      this._chunkTimer.cancel().catch(Cu.reportError);
    }
    if (this._sleepTimer) {
      this._sleepTimer.fire().catch(Cu.reportError);
    }
  }

  /**
   * Adds a match returned from a provider to the results set.
   * @param {object} provider
   * @param {object} match
   */
  add(provider, match) {
    if (!(provider instanceof UrlbarProvider)) {
      throw new Error("Invalid provider passed to the add callback");
    }
    // Stop returning results as soon as we've been canceled.
    if (this.canceled) {
      return;
    }
    // Check if the result source should be filtered out. Pay attention to the
    // heuristic result though, that is supposed to be added regardless.
    if (!this.acceptableSources.includes(match.source) && !match.heuristic) {
      return;
    }

    // Filter out javascript results for safety. The provider is supposed to do
    // it, but we don't want to risk leaking these out.
    if (match.payload.url && match.payload.url.startsWith("javascript:") &&
        !this.context.searchString.startsWith("javascript:") &&
        UrlbarPrefs.get("filter.javascript")) {
      return;
    }

    this.context.results.push(match);

    let notifyResults = () => {
      if (this._chunkTimer) {
        this._chunkTimer.cancel().catch(Cu.reportError);
        delete this._chunkTimer;
      }
      this.muxer.sort(this.context);

      // Crop results to the requested number.
      logger.debug(`Cropping ${this.context.results.length} matches to ${this.context.maxResults}`);
      this.context.results = this.context.results.slice(0, this.context.maxResults);
      this.controller.receiveResults(this.context);
    };

    // If the provider is not of immediate type, chunk results, to improve the
    // dataflow and reduce UI flicker.
    if (provider.type == UrlbarUtils.PROVIDER_TYPE.IMMEDIATE) {
      notifyResults();
    } else if (!this._chunkTimer) {
      this._chunkTimer = new SkippableTimer(notifyResults, CHUNK_MATCHES_DELAY_MS);
    }
  }
}

/**
 * Class used to create a timer that can be manually fired, to immediately
 * invoke the callback, or canceled, as necessary.
 * Examples:
 *   let timer = new SkippableTimer();
 *   // Invokes the callback immediately without waiting for the delay.
 *   await timer.fire();
 *   // Cancel the timer, the callback won't be invoked.
 *   await timer.cancel();
 *   // Wait for the timer to have elapsed.
 *   await timer.promise;
 */
class SkippableTimer {
  /**
   * Creates a skippable timer for the given callback and time.
   * @param {function} callback To be invoked when requested
   * @param {number} time A delay in milliseconds to wait for
   */
  constructor(callback, time) {
    let timerPromise = new Promise(resolve => {
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._timer.initWithCallback(() => {
        logger.debug(`Elapsed ${time}ms timer`);
        resolve();
      }, time, Ci.nsITimer.TYPE_ONE_SHOT);
      logger.debug(`Started ${time}ms timer`);
    });

    let firePromise = new Promise(resolve => {
      this.fire = () => {
        logger.debug(`Skipped ${time}ms timer`);
        resolve();
        return this.promise;
      };
    });

    this.promise = Promise.race([timerPromise, firePromise]).then(() => {
      // If we've been canceled, don't call back.
      if (this._timer) {
        callback();
      }
    });
  }

  /**
   * Allows to cancel the timer and the callback won't be invoked.
   * It is not strictly necessary to await for this, the promise can just be
   * used to ensure all the internal work is complete.
   * @returns {promise} Resolved once all the cancelation work is complete.
   */
  cancel() {
    logger.debug(`Canceling timer for ${this._timer.delay}ms`);
    this._timer.cancel();
    delete this._timer;
    return this.fire();
  }
}

/**
 * Gets an array of the provider sources accepted for a given UrlbarQueryContext.
 * @param {UrlbarQueryContext} context The query context to examine
 * @returns {array} Array of accepted sources
 */
function getAcceptableMatchSources(context) {
  let acceptedSources = [];
  // There can be only one restrict token about sources.
  let restrictToken = context.tokens.find(t => [ UrlbarTokenizer.TYPE.RESTRICT_HISTORY,
                                                 UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK,
                                                 UrlbarTokenizer.TYPE.RESTRICT_TAG,
                                                 UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE,
                                                 UrlbarTokenizer.TYPE.RESTRICT_SEARCH,
                                               ].includes(t.type));
  let restrictTokenType = restrictToken ? restrictToken.type : undefined;
  for (let source of Object.values(UrlbarUtils.RESULT_SOURCE)) {
    // Skip sources that the context doesn't care about.
    if (context.sources && !context.sources.includes(source)) {
      continue;
    }
    // Check prefs and restriction tokens.
    switch (source) {
      case UrlbarUtils.RESULT_SOURCE.BOOKMARKS:
        if (restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_BOOKMARK ||
            restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_TAG ||
            (!restrictTokenType && UrlbarPrefs.get("suggest.bookmark"))) {
          acceptedSources.push(source);
        }
        break;
      case UrlbarUtils.RESULT_SOURCE.HISTORY:
        if (restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_HISTORY ||
            (!restrictTokenType && UrlbarPrefs.get("suggest.history"))) {
          acceptedSources.push(source);
        }
        break;
      case UrlbarUtils.RESULT_SOURCE.SEARCH:
        if (restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_SEARCH ||
            (!restrictTokenType && UrlbarPrefs.get("suggest.searches"))) {
          acceptedSources.push(source);
        }
        break;
      case UrlbarUtils.RESULT_SOURCE.TABS:
        if (restrictTokenType === UrlbarTokenizer.TYPE.RESTRICT_OPENPAGE ||
            (!restrictTokenType && UrlbarPrefs.get("suggest.openpage"))) {
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
  return acceptedSources;
}
