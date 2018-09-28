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
    let query = Object.seal(new Query(queryContext, controller, this.providers));
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
    // Track the delay timer.
    this.sleepResolve = Promise.resolve();
    this.sleepTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
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

    await new Promise(resolve => {
      let time = UrlbarPrefs.get("delay");
      this.sleepResolve = resolve;
      this.sleepTimer.initWithCallback(resolve, time, Ci.nsITimer.TYPE_ONE_SHOT);
    });

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
    this.sleepTimer.cancel();
    for (let providers of this.providers.values()) {
      for (let provider of providers.values()) {
        provider.cancelQuery(this.context);
      }
    }
    this.sleepResolve();
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
    // TODO:
    //  * coalesce results in timed chunks: we don't want to notify every single
    //    result as soon as it arrives, we'll rather collect results for a few
    //    ms, then send them
    //  * pass results to a muxer before sending them back to the controller.
    this.context.results.push(match);
    this.controller.receiveResults(this.context);
  }
}
