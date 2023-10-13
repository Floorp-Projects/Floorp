/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider class that is used for providers created by
 * extensions.
 */

import {
  SkippableTimer,
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

// The set of `UrlbarQueryContext` properties that aren't serializable.
const NONSERIALIZABLE_CONTEXT_PROPERTIES = new Set(["view"]);

/**
 * The browser.urlbar extension API allows extensions to create their own urlbar
 * providers.  The results from extension providers are integrated into the
 * urlbar view just like the results from providers that are built into Firefox.
 *
 * This class is the interface between the provider-related parts of the
 * browser.urlbar extension API implementation and our internal urlbar
 * implementation.  The API implementation should use this class to manage
 * providers created by extensions.  All extension providers must be instances
 * of this class.
 *
 * When an extension requires a provider, the API implementation should call
 * getOrCreate() to get or create it.  When an extension adds an event listener
 * related to a provider, the API implementation should call setEventListener()
 * to register its own event listener with the provider.
 */
export class UrlbarProviderExtension extends UrlbarProvider {
  /**
   * Returns the extension provider with the given name, creating it first if
   * it doesn't exist.
   *
   * @param {string} name
   *   The provider name.
   * @returns {UrlbarProviderExtension}
   *   The provider.
   */
  static getOrCreate(name) {
    let provider = lazy.UrlbarProvidersManager.getProvider(name);
    if (!provider) {
      provider = new UrlbarProviderExtension(name);
      lazy.UrlbarProvidersManager.registerProvider(provider);
    }
    return provider;
  }

  /**
   * Constructor.
   *
   * @param {string} name
   *   The provider's name.
   */
  constructor(name) {
    super();
    this._name = name;
    this._eventListeners = new Map();
    this.behavior = "inactive";
  }

  /**
   * The provider's name.
   *
   * @returns {string}
   */
  get name() {
    return this._name;
  }

  /**
   * The provider's type.  The type of extension providers is always
   * UrlbarUtils.PROVIDER_TYPE.EXTENSION.
   *
   * @returns {UrlbarUtils.PROVIDER_TYPE}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.EXTENSION;
  }

  /**
   * Whether the provider should be invoked for the given context.  If this
   * method returns false, the providers manager won't start a query with this
   * provider, to save on resources.
   *
   * @param {UrlbarQueryContext} context
   *   The query context object.
   * @returns {boolean}
   *   Whether this provider should be invoked for the search.
   */
  isActive(context) {
    return this.behavior != "inactive";
  }

  /**
   * Gets the provider's priority.
   *
   * @param {UrlbarQueryContext} context
   *   The query context object.
   * @returns {number}
   *   The provider's priority for the given query.
   */
  getPriority(context) {
    // We give restricting extension providers a very high priority so that they
    // normally override all built-in providers, but not Infinity so that we can
    // still override them if necessary.
    return this.behavior == "restricting" ? 999 : 0;
  }

  /**
   * Sets the listener function for an event.  The extension API implementation
   * should call this from its EventManager.register() implementations.  Since
   * EventManager.register() is called at most only once for each extension
   * event (the first time the extension adds a listener for the event), each
   * provider instance needs at most only one listener per event, and that's why
   * this method is named setEventListener instead of addEventListener.
   *
   * The given listener function may return a promise that's resolved once the
   * extension responds to the event, or if the event requires no response from
   * the extension, it may return a non-promise value (possibly nothing).
   *
   * To remove the previously set listener, call this method again but pass null
   * as the listener function.
   *
   * The event name should be one of the following:
   *
   *   behaviorRequested
   *     This event is fired when the provider's behavior is needed from the
   *     extension.  The listener should return a behavior string.
   *   queryCanceled
   *     This event is fired when an ongoing query is canceled.  The listener
   *     shouldn't return anything.
   *   resultsRequested
   *     This event is fired when the provider's results are needed from the
   *     extension.  The listener should return an array of results.
   *
   * @param {string} eventName
   *   The name of the event to listen to.
   * @param {Function} listener
   *   The function that will be called when the event is fired.
   */
  setEventListener(eventName, listener) {
    if (listener) {
      this._eventListeners.set(eventName, listener);
    } else {
      this._eventListeners.delete(eventName);
      if (!this._eventListeners.size) {
        lazy.UrlbarProvidersManager.unregisterProvider(this);
      }
    }
  }

  /**
   * This method is called by the providers manager before a query starts to
   * update each extension provider's behavior.  It fires the behaviorRequested
   * event.
   *
   * @param {UrlbarQueryContext} context
   *   The query context.
   */
  async updateBehavior(context) {
    let behavior = await this._notifyListener(
      "behaviorRequested",
      makeSerializable(context)
    );
    if (behavior) {
      this.behavior = behavior;
    }
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.  See the base UrlbarProvider class for more.
   *
   * @param {UrlbarResult} result The result whose view will be updated.
   * @param {Map} idsByName
   *   A Map from an element's name, as defined by the provider; to its ID in
   *   the DOM, as defined by the browser.
   * @returns {object} An object describing the view update.
   */
  async getViewUpdate(result, idsByName) {
    return this._notifyListener("getViewUpdate", result, idsByName);
  }

  /**
   * This method is called by the providers manager when a query starts to fetch
   * each extension provider's results.  It fires the resultsRequested event.
   *
   * @param {UrlbarQueryContext} context
   *   The query context.
   * @param {Function} addCallback
   *   The callback invoked by this method to add each result.
   */
  async startQuery(context, addCallback) {
    let extResults = await this._notifyListener(
      "resultsRequested",
      makeSerializable(context)
    );
    if (extResults) {
      for (let extResult of extResults) {
        let result = await this._makeUrlbarResult(context, extResult).catch(
          ex => this.logger.error(ex)
        );
        if (result) {
          addCallback(this, result);
        }
      }
    }
  }

  /**
   * This method is called by the providers manager when an ongoing query is
   * canceled.  It fires the queryCanceled event.
   *
   * @param {UrlbarQueryContext} context
   *   The query context.
   */
  cancelQuery(context) {
    this._notifyListener("queryCanceled", makeSerializable(context));
  }

  #pickResult(result, element) {
    let dynamicElementName = "";
    if (element && result.type == UrlbarUtils.RESULT_TYPE.DYNAMIC) {
      dynamicElementName = element.getAttribute("name");
    }
    this._notifyListener("resultPicked", result.payload, dynamicElementName);
  }

  onEngagement(state, queryContext, details, controller) {
    let { result, element } = details;
    // By design, the "resultPicked" extension event should not be fired when
    // the picked element has a URL.
    if (result?.providerName == this.name && !element?.dataset.url) {
      this.#pickResult(result, element);
    }

    this._notifyListener("engagement", controller.input.isPrivate, state);
  }

  /**
   * Calls a listener function set by the extension API implementation, if any.
   *
   * @param {string} eventName
   *   The name of the listener to call (i.e., the name of the event to fire).
   * @param {*} args
   *   Arguments to the listener function.
   * @returns {*}
   *   The value returned by the listener function, if any.
   */
  async _notifyListener(eventName, ...args) {
    let listener = this._eventListeners.get(eventName);
    if (!listener) {
      return undefined;
    }
    let result;
    try {
      result = listener(...args);
    } catch (error) {
      this.logger.error(error);
      return undefined;
    }
    if (result.catch) {
      // The result is a promise, so wait for it to be resolved.  Set up a timer
      // so that we're not stuck waiting forever.
      let timer = new SkippableTimer({
        name: "UrlbarProviderExtension notification timer",
        time: lazy.UrlbarPrefs.get("extension.timeout"),
        reportErrorOnTimeout: true,
        logger: this.logger,
      });
      result = await Promise.race([
        timer.promise,
        result.catch(ex => this.logger.error(ex)),
      ]);
      timer.cancel();
    }
    return result;
  }

  /**
   * Converts a plain-JS-object result created by the extension into a
   * UrlbarResult object.
   *
   * @param {UrlbarQueryContext} context
   *   The query context.
   * @param {object} extResult
   *   A plain JS object representing a result created by the extension.
   * @returns {UrlbarResult}
   *   The UrlbarResult object.
   */
  async _makeUrlbarResult(context, extResult) {
    // If the result is a search result, make sure its payload has a valid
    // `engine` property, which is the name of an engine, and which we use later
    // on to look up the nsISearchEngine.  We allow the extension to specify the
    // engine by its name, alias, or domain.  Prefer aliases over domains since
    // one domain can have many engines.
    if (extResult.type == "search") {
      let engine;
      if (extResult.payload.engine) {
        // Validate the engine name by looking it up.
        engine = Services.search.getEngineByName(extResult.payload.engine);
      } else if (extResult.payload.keyword) {
        // Look up the engine by its alias.
        engine = await lazy.UrlbarSearchUtils.engineForAlias(
          extResult.payload.keyword
        );
      } else if (extResult.payload.url) {
        // Look up the engine by its domain.
        let host;
        try {
          host = new URL(extResult.payload.url).hostname;
        } catch (err) {}
        if (host) {
          engine = (
            await lazy.UrlbarSearchUtils.enginesForDomainPrefix(host)
          )[0];
        }
      }
      if (!engine) {
        // No engine found.
        throw new Error("Invalid or missing engine specified by extension");
      }
      extResult.payload.engine = engine.name;
    }

    let type = UrlbarProviderExtension.RESULT_TYPES[extResult.type];
    if (type == UrlbarUtils.RESULT_TYPE.TIP) {
      extResult.payload.type ||= "extension";
      extResult.payload.helpL10n = {
        id: "urlbar-result-menu-tip-get-help",
      };
    }

    let result = new lazy.UrlbarResult(
      UrlbarProviderExtension.RESULT_TYPES[extResult.type],
      UrlbarProviderExtension.SOURCE_TYPES[extResult.source],
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        context.tokens,
        extResult.payload || {}
      )
    );
    if (extResult.heuristic && this.behavior == "restricting") {
      // The muxer chooses the final heuristic result by taking the first one
      // that claims to be the heuristic.  We don't want extensions to clobber
      // the default heuristic, so we allow this only if the provider is
      // restricting.
      result.heuristic = extResult.heuristic;
    }
    if (extResult.suggestedIndex !== undefined) {
      result.suggestedIndex = extResult.suggestedIndex;
    }
    return result;
  }
}

// Maps extension result type enums to internal result types.
UrlbarProviderExtension.RESULT_TYPES = {
  dynamic: UrlbarUtils.RESULT_TYPE.DYNAMIC,
  keyword: UrlbarUtils.RESULT_TYPE.KEYWORD,
  omnibox: UrlbarUtils.RESULT_TYPE.OMNIBOX,
  remote_tab: UrlbarUtils.RESULT_TYPE.REMOTE_TAB,
  search: UrlbarUtils.RESULT_TYPE.SEARCH,
  tab: UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
  tip: UrlbarUtils.RESULT_TYPE.TIP,
  url: UrlbarUtils.RESULT_TYPE.URL,
};

// Maps extension source type enums to internal source types.
UrlbarProviderExtension.SOURCE_TYPES = {
  bookmarks: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  history: UrlbarUtils.RESULT_SOURCE.HISTORY,
  local: UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
  network: UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
  search: UrlbarUtils.RESULT_SOURCE.SEARCH,
  tabs: UrlbarUtils.RESULT_SOURCE.TABS,
  actions: UrlbarUtils.RESULT_SOURCE.ACTIONS,
};

/**
 * Returns a copy of a query context stripped of non-serializable properties.
 * This is necessary because query contexts are passed to extensions where they
 * become `Query` objects, as defined in the urlbar extensions schema. The
 * WebExtensions framework automatically excludes serializable properties that
 * aren't defined in the schema, but it chokes on non-serializable properties.
 *
 * @param {UrlbarQueryContext} context
 *   The query context.
 * @returns {object}
 *   A copy of `context` with only serializable properties.
 */
function makeSerializable(context) {
  return Object.fromEntries(
    Object.entries(context).filter(
      ([key]) => !NONSERIALIZABLE_CONTEXT_PROPERTIES.has(key)
    )
  );
}
