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
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Class used to create the provider.
 */
class ProviderTokenAliasEngines extends UrlbarProvider {
  constructor() {
    super();
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
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
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
    let instance = this.queryInstance;
    // Once the user starts typing a search string after the token, we hand off
    // suggestions to UrlbarProviderSearchSuggestions.
    if (
      !queryContext.searchString.startsWith("@") ||
      queryContext.tokens.length != 1
    ) {
      return false;
    }

    this._engines = await UrlbarSearchUtils.tokenAliasEngines();
    if (!this._engines.length) {
      return false;
    }

    // Check the query was not canceled while this executed.
    if (instance != this.queryInstance) {
      return false;
    }

    if (queryContext.searchString.trim() == "@") {
      return true;
    }

    // If there's no engine associated with the searchString, then we don't want
    // to block other kinds of results.
    if (UrlbarPrefs.get("autoFill") && queryContext.allowAutofill) {
      this._autofillResult = this._getAutofillResult(queryContext);
      if (this._autofillResult) {
        return true;
      }
    }

    return false;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   */
  async startQuery(queryContext, addCallback) {
    if (!this._engines || !this._engines.length) {
      return;
    }

    if (queryContext.searchString.trim() == "@") {
      for (let { engine, tokenAliases } of this._engines) {
        let result = new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.SEARCH,
          UrlbarUtils.RESULT_SOURCE.SEARCH,
          ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
            keyword: [tokenAliases[0], UrlbarUtils.HIGHLIGHT.TYPED],
            query: ["", UrlbarUtils.HIGHLIGHT.TYPED],
            icon: engine.iconURI ? engine.iconURI.spec : "",
            keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
          })
        );
        addCallback(this, result);
      }
    } else if (this._autofillResult) {
      addCallback(this, this._autofillResult);
    }
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
    delete this._autofillResult;
  }

  _getAutofillResult(queryContext) {
    let token = queryContext.tokens[0];
    // The user is typing a specific engine. We should show a heuristic result.
    for (let { engine, tokenAliases } of this._engines) {
      for (let alias of tokenAliases) {
        if (alias.startsWith(token.lowerCaseValue)) {
          // We found a specific engine. We will add an autofill result.
          let aliasPreservingUserCase =
            token.value + alias.substr(token.value.length);
          let value = aliasPreservingUserCase + " ";
          let result = new UrlbarResult(
            UrlbarUtils.RESULT_TYPE.SEARCH,
            UrlbarUtils.RESULT_SOURCE.SEARCH,
            ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
              engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
              keyword: [aliasPreservingUserCase, UrlbarUtils.HIGHLIGHT.TYPED],
              query: ["", UrlbarUtils.HIGHLIGHT.TYPED],
              icon: engine.iconURI ? engine.iconURI.spec : "",
              keywordOffer: UrlbarUtils.KEYWORD_OFFER.HIDE,
              // For test interoperabilty with UrlbarProviderSearchSuggestions.
              suggestion: undefined,
              tailPrefix: undefined,
              tail: undefined,
              tailOffsetIndex: -1,
              isSearchHistory: false,
            })
          );
          result.heuristic = true;
          result.autofill = {
            value,
            selectionStart: queryContext.searchString.length,
            selectionEnd: value.length,
          };
          return result;
        }
      }
    }
    return null;
  }
}

var UrlbarProviderTokenAliasEngines = new ProviderTokenAliasEngines();
