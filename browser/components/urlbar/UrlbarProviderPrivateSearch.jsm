/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider returning a private search entry.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderPrivateSearch"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SkippableTimer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Provider.PrivateSearch")
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "separatePrivateDefaultUIEnabled",
  "browser.search.separatePrivateDefault.ui.enabled",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "separatePrivateDefault",
  "browser.search.separatePrivateDefault",
  false
);

/**
 * Class used to create the provider.
 */
class ProviderPrivateSearch extends UrlbarProvider {
  constructor() {
    super();
    // Maps the open tabs by userContextId.
    this.openTabs = new Map();
    // Maps the running queries by queryContext.
    this.queries = new Map();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "PrivateSearch";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return (
      separatePrivateDefaultUIEnabled &&
      !queryContext.isPrivate &&
      queryContext.tokens.length
    );
  }

  /**
   * Whether this provider wants to restrict results to just itself.
   * Other providers won't be invoked, unless this provider doesn't
   * support the current query.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider wants to restrict results.
   */
  isRestricting(queryContext) {
    return false;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        match.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    logger.info(`Starting query for ${queryContext.searchString}`);

    let searchString = queryContext.searchString.trim();
    if (
      queryContext.tokens.some(
        t => t.type == UrlbarTokenizer.TYPE.RESTRICT_SEARCH
      )
    ) {
      if (queryContext.tokens.length == 1) {
        // There's only the restriction token, bail out.
        return;
      }
      // Remove the restriction char from the search string.
      searchString = queryContext.tokens
        .filter(t => t.type != UrlbarTokenizer.TYPE.RESTRICT_SEARCH)
        .map(t => t.value)
        .join(" ");
    }

    let instance = {};
    this.queries.set(queryContext, instance);

    let engine = queryContext.engineName
      ? Services.search.getEngineByName(queryContext.engineName)
      : await Services.search.getDefaultPrivate();
    let isPrivateEngine =
      separatePrivateDefault && engine != (await Services.search.getDefault());
    logger.info(`isPrivateEngine: ${isPrivateEngine}`);

    // This is a delay added before returning results, to avoid flicker.
    // Our result must appear only when all results are searches, but if search
    // results arrive first, then the muxer would insert our result and then
    // immediately remove it when non-search results arrive.
    await new SkippableTimer({
      name: "ProviderPrivateSearch",
      time: 100,
      logger,
    }).promise;

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
        query: [searchString, UrlbarUtils.HIGHLIGHT.TYPED],
        icon: [engine.iconURI ? engine.iconURI.spec : null],
        inPrivateWindow: true,
        isPrivateEngine,
      })
    );
    result.suggestedIndex = 1;
    addCallback(this, result);
    this.queries.delete(queryContext);
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

var UrlbarProviderPrivateSearch = new ProviderPrivateSearch();
