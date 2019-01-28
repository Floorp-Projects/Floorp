/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarUtils singleton, which contains constants and
 * helper functions that are useful to all components of the urlbar.
 */

var EXPORTED_SYMBOLS = [
  "UrlbarMuxer",
  "UrlbarProvider",
  "UrlbarQueryContext",
  "UrlbarUtils",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BinarySearch: "resource://gre/modules/BinarySearch.jsm",
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
  // PROVIDER_TYPE and RESULT_TYPE
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

  // Defines UrlbarResult types.
  RESULT_TYPE: {
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

  /**
   * Returns a list of all the token substring matches in a string.  Each match
   * in the list is a tuple: [matchIndex, matchLength].  matchIndex is the index
   * in the string of the match, and matchLength is the length of the match.
   *
   * @param {array} tokens The tokens to search for.
   * @param {string} str The string to match against.
   * @returns {array} An array: [
   *            [matchIndex_0, matchLength_0],
   *            [matchIndex_1, matchLength_1],
   *            ...
   *            [matchIndex_n, matchLength_n]
   *          ].
   *          The array is sorted by match indexes ascending.
   */
  getTokenMatches(tokens, str) {
    return tokens.reduce((matches, token) => {
      let index = 0;
      while (index >= 0) {
        index = str.indexOf(token.value, index);
        if (index >= 0) {
          let match = [index, token.value.length];
          let matchesIndex = BinarySearch.insertionIndexOf((a, b) => {
            return a[0] - b[0];
          }, matches, match);
          matches.splice(matchesIndex, 0, match);
          index += token.value.length;
        }
      }
      return matches;
    }, []);
  },
};

/**
 * UrlbarQueryContext defines a user's autocomplete input from within the urlbar.
 * It supplements it with details of how the search results should be obtained
 * and what they consist of.
 */
class UrlbarQueryContext {
  /**
   * Constructs the UrlbarQueryContext instance.
   *
   * @param {object} options
   *   The initial options for UrlbarQueryContext.
   * @param {string} options.searchString
   *   The string the user entered in autocomplete. Could be the empty string
   *   in the case of the user opening the popup via the mouse.
   * @param {number} options.lastKey
   *   The last key the user entered (as a key code). Could be null if the search
   *   was started via the mouse.
   * @param {boolean} options.isPrivate
   *   Set to true if this query was started from a private browsing window.
   * @param {number} options.maxResults
   *   The maximum number of results that will be displayed for this query.
   * @param {boolean} options.enableAutofill
   *   Whether or not to include autofill results.
   */
  constructor(options = {}) {
    this._checkRequiredOptions(options, [
      "enableAutofill",
      "isPrivate",
      "lastKey",
      "maxResults",
      "searchString",
    ]);

    if (isNaN(parseInt(options.maxResults))) {
      throw new Error(`Invalid maxResults property provided to UrlbarQueryContext`);
    }

    if (options.providers &&
        (!Array.isArray(options.providers) || !options.providers.length)) {
      throw new Error(`Invalid providers list`);
    }

    if (options.sources &&
        (!Array.isArray(options.sources) || !options.sources.length)) {
      throw new Error(`Invalid sources list`);
    }
  }

  /**
   * Checks the required options, saving them as it goes.
   *
   * @param {object} options The options object to check.
   * @param {array} optionNames The names of the options to check for.
   * @throws {Error} Throws if there is a missing option.
   */
  _checkRequiredOptions(options, optionNames) {
    for (let optionName of optionNames) {
      if (!(optionName in options)) {
        throw new Error(`Missing or empty ${optionName} provided to UrlbarQueryContext`);
      }
      this[optionName] = options[optionName];
    }
  }
}

/**
 * Base class for a muxer.
 * The muxer scope is to sort a given list of matches.
 */
class UrlbarMuxer {
  /**
   * Unique name for the muxer, used by the context to sort matches.
   * Not using a unique name will cause the newest registration to win.
   * @abstract
   */
  get name() {
    return "UrlbarMuxerBase";
  }
  /**
   * Sorts queryContext matches in-place.
   * @param {UrlbarQueryContext} queryContext the context to sort matches for.
   * @abstract
   */
  sort(queryContext) {
    throw new Error("Trying to access the base class, must be overridden");
  }
}

/**
 * Base class for a provider.
 * The provider scope is to query a datasource and return matches from it.
 */
class UrlbarProvider {
  /**
   * Unique name for the provider, used by the context to filter on providers.
   * Not using a unique name will cause the newest registration to win.
   * @abstract
   */
  get name() {
    return "UrlbarProviderBase";
  }
  /**
   * The type of the provider, must be one of UrlbarUtils.PROVIDER_TYPE.
   * @abstract
   */
  get type() {
    throw new Error("Trying to access the base class, must be overridden");
  }
  /**
   * List of UrlbarUtils.MATCH_SOURCE, representing the data sources used by
   * the provider.
   * @abstract
   */
  get sources() {
    throw new Error("Trying to access the base class, must be overridden");
  }
  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   * @abstract
   */
  startQuery(queryContext, addCallback) {
    throw new Error("Trying to access the base class, must be overridden");
  }
  /**
   * Cancels a running query,
   * @param {UrlbarQueryContext} queryContext the query context object to cancel
   *        query for.
   * @abstract
   */
  cancelQuery(queryContext) {
    throw new Error("Trying to access the base class, must be overridden");
  }
}
