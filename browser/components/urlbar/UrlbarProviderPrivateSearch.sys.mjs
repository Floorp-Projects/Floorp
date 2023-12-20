/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider returning a private search entry.
 */

import {
  SkippableTimer,
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
});

/**
 * Class used to create the provider.
 */
class ProviderPrivateSearch extends UrlbarProvider {
  constructor() {
    super();
    // Maps the open tabs by userContextId.
    this.openTabs = new Map();
  }

  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "PrivateSearch";
  }

  /**
   * Returns the type of this provider.
   *
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
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
    return (
      lazy.UrlbarSearchUtils.separatePrivateDefaultUIEnabled &&
      !queryContext.isPrivate &&
      queryContext.tokens.length
    );
  }

  /**
   * Starts querying.
   *
   * @param {object} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        match.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    let searchString = queryContext.trimmedSearchString;
    if (
      queryContext.tokens.some(
        t => t.type == lazy.UrlbarTokenizer.TYPE.RESTRICT_SEARCH
      )
    ) {
      if (queryContext.tokens.length == 1) {
        // There's only the restriction token, bail out.
        return;
      }
      // Remove the restriction char from the search string.
      searchString = queryContext.tokens
        .filter(t => t.type != lazy.UrlbarTokenizer.TYPE.RESTRICT_SEARCH)
        .map(t => t.value)
        .join(" ");
    }

    let instance = this.queryInstance;

    let engine = queryContext.searchMode?.engineName
      ? Services.search.getEngineByName(queryContext.searchMode.engineName)
      : await Services.search.getDefaultPrivate();
    let isPrivateEngine =
      lazy.UrlbarSearchUtils.separatePrivateDefault &&
      engine != (await Services.search.getDefault());
    this.logger.info(`isPrivateEngine: ${isPrivateEngine}`);

    // This is a delay added before returning results, to avoid flicker.
    // Our result must appear only when all results are searches, but if search
    // results arrive first, then the muxer would insert our result and then
    // immediately remove it when non-search results arrive.
    await new SkippableTimer({
      name: "ProviderPrivateSearch",
      time: 100,
      logger: this.logger,
    }).promise;

    if (instance != this.queryInstance) {
      return;
    }

    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
        query: [searchString, UrlbarUtils.HIGHLIGHT.NONE],
        icon: engine.getIconURL(),
        inPrivateWindow: true,
        isPrivateEngine,
      })
    );
    result.suggestedIndex = 1;
    addCallback(this, result);
  }
}

export var UrlbarProviderPrivateSearch = new ProviderPrivateSearch();
