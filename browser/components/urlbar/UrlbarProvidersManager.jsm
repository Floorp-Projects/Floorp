/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a component used to register search providers and manage
 * the connection between such providers and a UrlbarController.
 */

var EXPORTED_SYMBOLS = ["UrlbarProvidersManager"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://modules/PlacesUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Places.Urlbar.ProvidersManager"));

// List of available local providers, each is implemented in its own jsm module
// and will track different queries internally by queryContext.
var localProviderModules = {
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.jsm",
};

// To improve dataflow and reduce UI work, when a match is added by a
// non-immediate provider, we notify it to the controller after a delay, so
// that we can chunk matches coming in that timeframe into a single call.
const CHUNK_MATCHES_DELAY_MS = 16;

/**
 * Class used to create a manager.
 * The manager is responsible to keep a list of providers, instantiate query
 * objects and pass those to the providers.
 */
class ProvidersManager {
  constructor() {
    // Tracks the available providers.
    // This is a double map, first it maps by PROVIDER_TYPE, then
    // registerProvider maps by provider.name: { type: { name: provider }}
    this.providers = new Map();
    for (let type of Object.values(UrlbarUtils.PROVIDER_TYPE)) {
      this.providers.set(type, new Map());
    }
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
  }

  /**
   * Registers a provider object with the manager.
   * @param {object} provider
   */
  registerProvider(provider) {
    logger.info(`Registering provider ${provider.name}`);
    if (!Object.values(UrlbarUtils.PROVIDER_TYPE).includes(provider.type)) {
      throw new Error(`Unknown provider type ${provider.type}`);
    }
    this.providers.get(provider.type).set(provider.name, provider);
  }

  /**
   * Unregisters a previously registered provider object.
   * @param {object} provider
   */
  unregisterProvider(provider) {
    logger.info(`Unregistering provider ${provider.name}`);
    this.providers.get(provider.type).delete(provider.name);
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {object} controller a UrlbarController instance
   */
  async startQuery(queryContext, controller) {
    logger.info(`Query start ${queryContext.searchString}`);
    let query = new Query(queryContext, controller, this.providers);
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
   * @param {object} providers
   *        Map of all the providers by type and name
   */
  constructor(queryContext, controller, providers) {
    this.context = queryContext;
    this.context.results = [];
    this.controller = controller;
    this.providers = providers;
    this.started = false;
    this.canceled = false;
    this.complete = false;
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

    let promises = [];
    for (let provider of this.providers.get(UrlbarUtils.PROVIDER_TYPE.IMMEDIATE).values()) {
      if (this.canceled) {
        break;
      }
      promises.push(provider.startQuery(this.context, this.add));
    }

    // Tracks the delay timer. We will fire (in this specific case, cancel would
    // do the same, since the callback is empty) the timer when the search is
    // canceled, unblocking start().
    this._sleepTimer = new SkippableTimer(() => {}, UrlbarPrefs.get("delay"));
    await this._sleepTimer.promise;

    for (let providerType of [UrlbarUtils.PROVIDER_TYPE.NETWORK,
                              UrlbarUtils.PROVIDER_TYPE.PROFILE,
                              UrlbarUtils.PROVIDER_TYPE.EXTENSION]) {
      for (let provider of this.providers.get(providerType).values()) {
        if (this.canceled) {
          break;
        }
        promises.push(provider.startQuery(this.context, this.add.bind(this)));
      }
    }

    await Promise.all(promises.map(p => p.catch(Cu.reportError)));

    if (this._chunkTimer) {
      // All the providers are done returning results, so we can stop chunking.
      await this._chunkTimer.fire();
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
    for (let providers of this.providers.values()) {
      for (let provider of providers.values()) {
        provider.cancelQuery(this.context);
      }
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
    // Stop returning results as soon as we've been canceled.
    if (this.canceled) {
      return;
    }
    this.context.results.push(match);


    let notifyResults = () => {
      if (this._chunkTimer) {
        this._chunkTimer.cancel().catch(Cu.reportError);
        delete this._chunkTimer;
      }
      // TODO:
      //  * pass results to a muxer before sending them back to the controller.
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
