/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers token alias engines.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
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
    // Beats UrlbarProviderSearchSuggestions and UrlbarProviderPlaces.
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

    // This is usually reset on canceling or completing the query, but since we
    // query in isActive, it may not have been canceled by the previous call.
    // It is an object with values { result: UrlbarResult, instance: Query }.
    this._autofillData = null;

    // Once the user starts typing a search string after the token, we hand off
    // suggestions to UrlbarProviderSearchSuggestions.
    if (
      !queryContext.searchString.startsWith("@") ||
      queryContext.tokens.length != 1
    ) {
      return false;
    }

    // Do not show token alias results in search mode.
    if (queryContext.searchMode) {
      return false;
    }

    this._engines = await lazy.UrlbarSearchUtils.tokenAliasEngines();
    if (!this._engines.length) {
      return false;
    }

    // Check the query was not canceled while this executed.
    if (instance != this.queryInstance) {
      return false;
    }

    if (queryContext.trimmedSearchString == "@") {
      return true;
    }

    // If the user is typing a potential engine name, autofill it.
    if (lazy.UrlbarPrefs.get("autoFill") && queryContext.allowAutofill) {
      let result = this._getAutofillResult(queryContext);
      if (result) {
        this._autofillData = { result, instance };
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

    if (
      this._autofillData &&
      this._autofillData.instance == this.queryInstance
    ) {
      addCallback(this, this._autofillData.result);
    }

    for (let { engine, tokenAliases } of this._engines) {
      if (
        tokenAliases[0].startsWith(queryContext.trimmedSearchString) &&
        engine.name != this._autofillData?.result.payload.engine
      ) {
        let result = new lazy.UrlbarResult(
          UrlbarUtils.RESULT_TYPE.SEARCH,
          UrlbarUtils.RESULT_SOURCE.SEARCH,
          ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
            keyword: [tokenAliases[0], UrlbarUtils.HIGHLIGHT.TYPED],
            query: ["", UrlbarUtils.HIGHLIGHT.TYPED],
            icon: engine.iconURI?.spec,
            providesSearchMode: true,
          })
        );
        addCallback(this, result);
      }
    }

    this._autofillData = null;
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
    if (this._autofillData?.instance == this.queryInstance) {
      this._autofillData = null;
    }
  }

  _getAutofillResult(queryContext) {
    let lowerCaseSearchString = queryContext.searchString.toLowerCase();

    // The user is typing a specific engine. We should show a heuristic result.
    for (let { engine, tokenAliases } of this._engines) {
      for (let alias of tokenAliases) {
        if (alias.startsWith(lowerCaseSearchString)) {
          // We found the engine.

          // Stop adding an autofill result once the user has typed the full
          // alias followed by a space. We enter search mode at that point.
          if (
            lowerCaseSearchString.startsWith(alias) &&
            lazy.UrlbarTokenizer.REGEXP_SPACES_START.test(
              lowerCaseSearchString.substring(alias.length)
            )
          ) {
            return null;
          }

          // Add an autofill result.  Append a space so the user can hit enter
          // or the right arrow key and immediately start typing their query.
          let aliasPreservingUserCase =
            queryContext.searchString +
            alias.substr(queryContext.searchString.length);
          let value = aliasPreservingUserCase + " ";
          let result = new lazy.UrlbarResult(
            UrlbarUtils.RESULT_TYPE.SEARCH,
            UrlbarUtils.RESULT_SOURCE.SEARCH,
            ...lazy.UrlbarResult.payloadAndSimpleHighlights(
              queryContext.tokens,
              {
                engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
                keyword: [aliasPreservingUserCase, UrlbarUtils.HIGHLIGHT.TYPED],
                query: ["", UrlbarUtils.HIGHLIGHT.TYPED],
                icon: engine.iconURI?.spec,
                providesSearchMode: true,
              }
            )
          );

          // We set suggestedIndex = 0 instead of the heuristic because we
          // don't want this result to be automatically selected. That way,
          // users can press Tab to select the result, building on their
          // muscle memory from tab-to-search.
          result.suggestedIndex = 0;

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

export var UrlbarProviderTokenAliasEngines = new ProviderTokenAliasEngines();
