/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers token alias engines.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderTokenAliasEngines"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesSearchAutocompleteProvider:
    "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Provider.TokenAliasEngines")
);

/**
 * Class used to create the provider.
 */
class ProviderTokenAliasEngines extends UrlbarProvider {
  constructor() {
    super();
    // Maps the running queries by queryContext.
    this.queries = new Map();
    this._engines = [];
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "TokenAliasEngines";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  get PRIORITY() {
    // Beats UrlbarProviderSearchSuggestions and UnifiedComplete.
    return 1;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  async isActive(queryContext) {
    this._engines = [];
    if (queryContext.searchString.trim() == "@") {
      this._engines = await PlacesSearchAutocompleteProvider.tokenAliasEngines();
    }

    return this._engines.length;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   */
  async startQuery(queryContext, addCallback) {
    logger.info(`Starting query for ${queryContext.searchString}`);
    let instance = {};
    this.queries.set(queryContext, instance);

    // Even though startQuery doesn't wait on any Promises, we check
    // this.queries.has(queryContext) because isActive waits but we can't check
    // the same condition there.
    if (
      !this._engines ||
      !this._engines.length ||
      !this.queries.has(queryContext)
    ) {
      return;
    }

    for (let { engine, tokenAliases } of this._engines) {
      let result = new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
          keyword: [tokenAliases[0], UrlbarUtils.HIGHLIGHT.TYPED],
          query: ["", UrlbarUtils.HIGHLIGHT.TYPED],
          icon: engine.iconURI ? engine.iconURI.spec : null,
          keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
        })
      );
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
    return this.PRIORITY;
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    logger.info(`Canceling query for ${queryContext.searchString}`);
    this.queries.delete(queryContext);
  }
}

var UrlbarProviderTokenAliasEngines = new ProviderTokenAliasEngines();
