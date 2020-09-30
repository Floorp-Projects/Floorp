/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers a search engine when the user is
 * typing a search engine domain.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderTabToSearch"];

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
class ProviderTabToSearch extends UrlbarProvider {
  constructor() {
    super();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "TabToSearch";
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
  async isActive(queryContext) {
    return (
      !queryContext.searchMode &&
      UrlbarPrefs.get("update2") &&
      UrlbarPrefs.get("update2.tabToComplete")
    );
  }

  /**
   * Gets the provider's priority.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   */
  getPriority(queryContext) {
    return 0;
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    // engineForDomainPrefix only matches against engine domains.
    // Remove trailing slashes and www. from the search string and check if the
    // resulting string is worth matching.
    // The muxer will verify that the found result matches the autofilled value.
    let [searchStr] = UrlbarUtils.stripPrefixAndTrim(
      queryContext.searchString,
      {
        stripWww: true,
        trimSlash: true,
      }
    );

    let engine = await UrlbarSearchUtils.engineForDomainPrefix(searchStr);
    if (!engine) {
      return;
    }

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        engine: engine.name,
        url: engine.getResultDomain(),
        keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
        icon: engine.iconURI?.spec,
        query: "",
      })
    );
    result.suggestedIndex = 1;
    addCallback(this, result);
  }
}

var UrlbarProviderTabToSearch = new ProviderTabToSearch();
