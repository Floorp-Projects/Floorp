/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider class that is used for providers created by
 * extensions using the `omnibox` API.
 */

import {
  SkippableTimer,
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionSearchHandler:
    "resource://gre/modules/ExtensionSearchHandler.sys.mjs",

  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

// After this time, we'll give up waiting for the extension to return matches.
const MAXIMUM_ALLOWED_EXTENSION_TIME_MS = 3000;

/**
 * This provider handles results returned by extensions using the WebExtensions
 * Omnibox API. If the user types a registered keyword, we send subsequent
 * keystrokes to the extension.
 */
class ProviderOmnibox extends UrlbarProvider {
  constructor() {
    super();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "Omnibox";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  /**
   * Whether the provider should be invoked for the given context.  If this
   * method returns false, the providers manager won't start a query with this
   * provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context object.
   * @returns {boolean}
   *   Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    if (
      queryContext.tokens[0] &&
      queryContext.tokens[0].value.length &&
      lazy.ExtensionSearchHandler.isKeywordRegistered(
        queryContext.tokens[0].value
      ) &&
      UrlbarUtils.substringAfter(
        queryContext.searchString,
        queryContext.tokens[0].value
      )
    ) {
      return true;
    }

    // We need to handle cancellation here since isActive is called once per
    // query but cancelQuery can be called multiple times per query.
    // The frequent cancels can cause the extension's state to drift from the
    // provider's state.
    if (lazy.ExtensionSearchHandler.hasActiveInputSession()) {
      lazy.ExtensionSearchHandler.handleInputCancelled();
    }

    return false;
  }

  /**
   * Gets the provider's priority.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context object.
   * @returns {number}
   *   The provider's priority for the given query.
   */
  getPriority(queryContext) {
    return 0;
  }

  /**
   * This method is called by the providers manager when a query starts to fetch
   * each extension provider's results.  It fires the resultsRequested event.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context object.
   * @param {function} addCallback
   *   The callback invoked by this method to add each result.
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;

    // Fetch heuristic result.
    let keyword = queryContext.tokens[0].value;
    let description = lazy.ExtensionSearchHandler.getDescription(keyword);
    let heuristicResult = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.OMNIBOX,
      UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        title: [description, UrlbarUtils.HIGHLIGHT.TYPED],
        content: [queryContext.searchString, UrlbarUtils.HIGHLIGHT.TYPED],
        keyword: [queryContext.tokens[0].value, UrlbarUtils.HIGHLIGHT.TYPED],
        icon: UrlbarUtils.ICON.EXTENSION,
      })
    );
    heuristicResult.heuristic = true;
    addCallback(this, heuristicResult);

    // Fetch non-heuristic results.
    let data = {
      keyword,
      text: queryContext.searchString,
      inPrivateWindow: queryContext.isPrivate,
    };
    this._resultsPromise = lazy.ExtensionSearchHandler.handleSearch(
      data,
      suggestions => {
        if (instance != this.queryInstance) {
          return;
        }
        for (let suggestion of suggestions) {
          let content = `${queryContext.tokens[0].value} ${suggestion.content}`;
          if (content == heuristicResult.payload.content) {
            continue;
          }
          let result = new lazy.UrlbarResult(
            UrlbarUtils.RESULT_TYPE.OMNIBOX,
            UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
            ...lazy.UrlbarResult.payloadAndSimpleHighlights(
              queryContext.tokens,
              {
                title: [suggestion.description, UrlbarUtils.HIGHLIGHT.TYPED],
                content: [content, UrlbarUtils.HIGHLIGHT.TYPED],
                keyword: [
                  queryContext.tokens[0].value,
                  UrlbarUtils.HIGHLIGHT.TYPED,
                ],
                icon: UrlbarUtils.ICON.EXTENSION,
              }
            )
          );
          addCallback(this, result);
        }
      }
    );

    // Since the extension has no way to signal when it's done pushing results,
    // we add a timer racing with the addition.
    let timeoutPromise = new SkippableTimer({
      name: "ProviderOmnibox",
      time: MAXIMUM_ALLOWED_EXTENSION_TIME_MS,
      logger: this.logger,
    }).promise;
    await Promise.race([timeoutPromise, this._resultsPromise]).catch(ex =>
      this.logger.error(ex)
    );
  }
}

export var UrlbarProviderOmnibox = new ProviderOmnibox();
