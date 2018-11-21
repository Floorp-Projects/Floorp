/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarUtils singleton, which contains constants and
 * helper functions that are useful to all components of the urlbar.
 */

var EXPORTED_SYMBOLS = ["UrlbarUtils"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

var UrlbarUtils = {
  // Values for browser.urlbar.insertMethod
  INSERTMETHOD: {
    // Just append new results.
    APPEND: 0,
    // Merge previous and current results if search strings are related.
    MERGE_RELATED: 1,
    // Always merge previous and current results.
    MERGE: 2,
  },

  // Extensions are allowed to add suggestions if they have registered a keyword
  // with the omnibox API. This is the maximum number of suggestions an extension
  // is allowed to add for a given search string.
  // This value includes the heuristic result.
  MAXIMUM_ALLOWED_EXTENSION_MATCHES: 6,

  // This is used by UnifiedComplete, the new implementation will use
  // PROVIDER_TYPE and MATCH_TYPE
  MATCH_GROUP: {
    HEURISTIC: "heuristic",
    GENERAL: "general",
    SUGGESTION: "suggestion",
    EXTENSION: "extension",
  },

  // Defines provider types.
  PROVIDER_TYPE: {
    // Should be executed immediately, because it returns heuristic results
    // that must be handled to the user asap.
    IMMEDIATE: 1,
    // Can be delayed, contains results coming from the session or the profile.
    PROFILE: 2,
    // Can be delayed, contains results coming from the network.
    NETWORK: 3,
    // Can be delayed, contains results coming from unknown sources.
    EXTENSION: 4,
  },

  // Defines UrlbarMatch types.
  MATCH_TYPE: {
    // An open tab.
    // Payload: { icon, url, userContextId }
    TAB_SWITCH: 1,
    // A search suggestion or engine.
    // Payload: { icon, suggestion, keyword, query }
    SEARCH: 2,
    // A common url/title tuple, may be a bookmark with tags.
    // Payload: { icon, url, title, tags }
    URL: 3,
    // A bookmark keyword.
    // Payload: { icon, url, keyword, postData }
    KEYWORD: 4,
    // A WebExtension Omnibox match.
    // Payload: { icon, keyword, title, content }
    OMNIBOX: 5,
    // A tab from another synced device.
    // Payload: { url, icon, device, title }
    REMOTE_TAB: 6,
  },

  // This defines the source of matches returned by a provider. Each provider
  // can return matches from more than one source. This is used by the
  // ProvidersManager to decide which providers must be queried and which
  // matches can be returned.
  MATCH_SOURCE: {
    BOOKMARKS: 1,
    HISTORY: 2,
    SEARCH: 3,
    TABS: 4,
    OTHER_LOCAL: 5,
    OTHER_NETWORK: 6,
  },

  /**
   * Adds a url to history as long as it isn't in a private browsing window,
   * and it is valid.
   *
   * @param {string} url The url to add to history.
   * @param {nsIDomWindow} window The window from where the url is being added.
   */
  addToUrlbarHistory(url, window) {
    if (!PrivateBrowsingUtils.isWindowPrivate(window) &&
        url &&
        !url.includes(" ") &&
        !/[\x00-\x1F]/.test(url)) // eslint-disable-line no-control-regex
      PlacesUIUtils.markPageAsTyped(url);
  },

  /**
   * Given a string, will generate a more appropriate urlbar value if a Places
   * keyword or a search alias is found at the beginning of it.
   *
   * @param {string} url
   *        A string that may begin with a keyword or an alias.
   *
   * @returns {Promise}
   * @resolves { url, postData, mayInheritPrincipal }. If it's not possible
   *           to discern a keyword or an alias, url will be the input string.
   */
  async getShortcutOrURIAndPostData(url) {
    let mayInheritPrincipal = false;
    let postData = null;
    // Split on the first whitespace.
    let [keyword, param = ""] = url.trim().split(/\s(.+)/, 2);

    if (!keyword) {
      return { url, postData, mayInheritPrincipal };
    }

    let engine = Services.search.getEngineByAlias(keyword);
    if (engine) {
      let submission = engine.getSubmission(param, null, "keyword");
      return { url: submission.uri.spec,
               postData: submission.postData,
               mayInheritPrincipal };
    }

    // A corrupt Places database could make this throw, breaking navigation
    // from the location bar.
    let entry = null;
    try {
      entry = await PlacesUtils.keywords.fetch(keyword);
    } catch (ex) {
      Cu.reportError(`Unable to fetch Places keyword "${keyword}": ${ex}`);
    }
    if (!entry || !entry.url) {
      // This is not a Places keyword.
      return { url, postData, mayInheritPrincipal };
    }

    try {
      [url, postData] =
        await BrowserUtils.parseUrlAndPostData(entry.url.href,
                                               entry.postData,
                                               param);
      if (postData) {
        postData = this.getPostDataStream(postData);
      }

      // Since this URL came from a bookmark, it's safe to let it inherit the
      // current document's principal.
      mayInheritPrincipal = true;
    } catch (ex) {
      // It was not possible to bind the param, just use the original url value.
    }

    return { url, postData, mayInheritPrincipal };
  },

  /**
   * Returns an input stream wrapper for the given post data.
   *
   * @param {string} postDataString The string to wrap.
   * @param {string} [type] The encoding type.
   * @returns {nsIInputStream} An input stream of the wrapped post data.
   */
  getPostDataStream(postDataString,
                    type = "application/x-www-form-urlencoded") {
    let dataStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
    dataStream.data = postDataString;

    let mimeStream = Cc["@mozilla.org/network/mime-input-stream;1"]
                       .createInstance(Ci.nsIMIMEInputStream);
    mimeStream.addHeader("Content-Type", type);
    mimeStream.setData(dataStream);
    return mimeStream.QueryInterface(Ci.nsIInputStream);
  },
};
