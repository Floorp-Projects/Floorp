/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers engines with aliases as heuristic
 * results.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderAliasEngines"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Class used to create the provider.
 */
class ProviderAliasEngines extends UrlbarProvider {
  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "AliasEngines";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  async isActive(queryContext) {
    return false;
    // TODO: Part 2: Enable this provider.
    return (
      (!queryContext.restrictSource ||
        queryContext.restrictSource == UrlbarTokenizer.RESTRICT.SEARCH) &&
      !queryContext.searchMode &&
      queryContext.tokens.length
    );
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   */
  async startQuery(queryContext, addCallback) {
    let alias = queryContext.tokens[0]?.value;
    let engine = await UrlbarSearchUtils.engineForAlias(alias);
    if (!engine) {
      return;
    }

    let query = UrlbarUtils.substringAfter(queryContext.searchString, alias);

    // Match an alias only when it has a space after it.  If there's no trailing
    // space, then continue to treat it as part of the search string.
    if (!UrlbarTokenizer.REGEXP_SPACES_START.test(query)) {
      return;
    }

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        engine: engine.name,
        keyword: alias,
        query: query.trimStart(),
        icon: engine.iconURI?.spec,
      })
    );
    result.heuristic = true;
    addCallback(this, result);
  }
}

var UrlbarProviderAliasEngines = new ProviderAliasEngines();
