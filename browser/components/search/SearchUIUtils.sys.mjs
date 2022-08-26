/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Various utilities for search related UI.
 */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "SearchUIUtilsL10n", () => {
  return new Localization(["browser/search.ftl", "branding/brand.ftl"]);
});

export var SearchUIUtils = {
  /**
   * Adds an open search engine and handles error UI.
   *
   * @param {string} locationURL
   *   The URL where the OpenSearch definition is located.
   * @param {string} image
   *   A URL string to an icon file to be used as the search engine's
   *   icon. This value may be overridden by an icon specified in the
   *   engine description file.
   * @param {object} browsingContext
   *  The browsing context any error prompt should be opened for.
   */
  async addOpenSearchEngine(locationURL, image, browsingContext) {
    try {
      await Services.search.addOpenSearchEngine(locationURL, image);
    } catch (ex) {
      let titleMsgName;
      let descMsgName;
      switch (ex.result) {
        case Ci.nsISearchService.ERROR_DUPLICATE_ENGINE:
          titleMsgName = "opensearch-error-duplicate-title";
          descMsgName = "opensearch-error-duplicate-desc";
          break;
        case Ci.nsISearchService.ERROR_ENGINE_CORRUPTED:
          titleMsgName = "opensearch-error-format-title";
          descMsgName = "opensearch-error-format-desc";
          break;
        default:
          // i.e. ERROR_DOWNLOAD_FAILURE
          titleMsgName = "opensearch-error-download-title";
          descMsgName = "opensearch-error-download-desc";
          break;
      }

      let [title, text] = await lazy.SearchUIUtilsL10n.formatValues([
        {
          id: titleMsgName,
        },
        {
          id: descMsgName,
          args: {
            "location-url": locationURL,
          },
        },
      ]);

      Services.prompt.alertBC(
        browsingContext,
        Ci.nsIPrompt.MODAL_TYPE_CONTENT,
        title,
        text
      );
      return false;
    }
    return true;
  },

  /**
   * Returns the URL to use for where to get more search engines.
   */
  get searchEnginesURL() {
    return Services.urlFormatter.formatURLPref(
      "browser.search.searchEnginesURL"
    );
  },
};
