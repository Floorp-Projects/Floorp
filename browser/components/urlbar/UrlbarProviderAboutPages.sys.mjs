/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that offers about pages.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutPagesUtils: "resource://gre/modules/AboutPagesUtils.jsm",
});

/**
 * Class used to create the provider.
 */
class ProviderAboutPages extends UrlbarProvider {
  /**
   * Unique name for the provider, used by the context to filter on providers.
   */
  get name() {
    return "AboutPages";
  }

  /**
   * The type of the provider, must be one of UrlbarUtils.PROVIDER_TYPE.
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
    return queryContext.trimmedSearchString.toLowerCase().startsWith("about:");
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   */
  startQuery(queryContext, addCallback) {
    let searchString = queryContext.trimmedSearchString.toLowerCase();
    for (const aboutUrl of lazy.AboutPagesUtils.visibleAboutUrls) {
      if (aboutUrl.startsWith(searchString)) {
        let result = new lazy.UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.HISTORY,
          ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            title: [aboutUrl, UrlbarUtils.HIGHLIGHT.TYPED],
            url: [aboutUrl, UrlbarUtils.HIGHLIGHT.TYPED],
            icon: UrlbarUtils.getIconForUrl(aboutUrl),
          })
        );
        addCallback(this, result);
      }
    }
  }
}

export var UrlbarProviderAboutPages = new ProviderAboutPages();
