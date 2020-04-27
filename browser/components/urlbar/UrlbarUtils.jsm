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
  "SkippableTimer",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
});

var UrlbarUtils = {
  // Extensions are allowed to add suggestions if they have registered a keyword
  // with the omnibox API. This is the maximum number of suggestions an extension
  // is allowed to add for a given search string.
  // This value includes the heuristic result.
  MAXIMUM_ALLOWED_EXTENSION_MATCHES: 6,

  // This is used by UnifiedComplete, the new implementation will use
  // PROVIDER_TYPE and RESULT_TYPE
  RESULT_GROUP: {
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
    TAB_SWITCH: 1,
    // A search suggestion or engine.
    SEARCH: 2,
    // A common url/title tuple, may be a bookmark with tags.
    URL: 3,
    // A bookmark keyword.
    KEYWORD: 4,
    // A WebExtension Omnibox result.
    OMNIBOX: 5,
    // A tab from another synced device.
    REMOTE_TAB: 6,
    // An actionable message to help the user with their query.
    TIP: 7,

    // When you add a new type, also add its schema to
    // UrlbarUtils.RESULT_PAYLOAD_SCHEMA below.  Also consider checking if
    // consumers of "urlbar-user-start-navigation" need updating.
  },

  // This defines the source of results returned by a provider. Each provider
  // can return results from more than one source. This is used by the
  // ProvidersManager to decide which providers must be queried and which
  // results can be returned.
  // If you add new source types, consider checking if consumers of
  // "urlbar-user-start-navigation" need update as well.
  RESULT_SOURCE: {
    BOOKMARKS: 1,
    HISTORY: 2,
    SEARCH: 3,
    TABS: 4,
    OTHER_LOCAL: 5,
    OTHER_NETWORK: 6,
  },

  // This defines icon locations that are commonly used in the UI.
  ICON: {
    // DEFAULT is defined lazily so it doesn't eagerly initialize PlacesUtils.
    SEARCH_GLASS: "chrome://browser/skin/search-glass.svg",
    TIP: "chrome://browser/skin/tip.svg",
  },

  // The number of results by which Page Up/Down move the selection.
  PAGE_UP_DOWN_DELTA: 5,

  // IME composition states.
  COMPOSITION: {
    NONE: 1,
    COMPOSING: 2,
    COMMIT: 3,
    CANCELED: 4,
  },

  // Limit the length of titles and URLs we display so layout doesn't spend too
  // much time building text runs.
  MAX_TEXT_LENGTH: 255,

  // Whether a result should be highlighted up to the point the user has typed
  // or after that point.
  HIGHLIGHT: {
    TYPED: 1,
    SUGGESTED: 2,
  },

  // Search results with keywords and empty queries are called "keyword offers".
  // When the user selects a keyword offer, the keyword followed by a space is
  // put in the input as a hint that the user can search using the keyword.
  // Depending on the use case, keyword-offer results can show or not show the
  // keyword itself.
  KEYWORD_OFFER: {
    NONE: 0,
    SHOW: 1,
    HIDE: 2,
  },

  // UnifiedComplete's autocomplete results store their titles and tags together
  // in their comments.  This separator is used to separate them.  When we
  // rewrite UnifiedComplete for quantumbar, we should stop using this old hack
  // and store titles and tags separately.  It's important that this be a
  // character that no title would ever have.  We use \x1F, the non-printable
  // unit separator.
  TITLE_TAGS_SEPARATOR: "\x1F",

  // Regex matching single word hosts with an optional port; no spaces, auth or
  // path-like chars are admitted.
  REGEXP_SINGLE_WORD: /^[^\s@:/?#]+(:\d+)?$/,

  /**
   * Returns the payload schema for the given type of result.
   *
   * @param {number} type One of the UrlbarUtils.RESULT_TYPE values.
   * @returns {object} The schema for the given type.
   */
  getPayloadSchema(type) {
    return UrlbarUtils.RESULT_PAYLOAD_SCHEMA[type];
  },

  /**
   * Adds a url to history as long as it isn't in a private browsing window,
   * and it is valid.
   *
   * @param {string} url The url to add to history.
   * @param {nsIDomWindow} window The window from where the url is being added.
   */
  addToUrlbarHistory(url, window) {
    if (
      !PrivateBrowsingUtils.isWindowPrivate(window) &&
      url &&
      !url.includes(" ") &&
      // eslint-disable-next-line no-control-regex
      !/[\x00-\x1F]/.test(url)
    ) {
      PlacesUIUtils.markPageAsTyped(url);
    }
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

    await Services.search.init();
    let engine = Services.search.getEngineByAlias(keyword);
    if (engine) {
      let submission = engine.getSubmission(param, null, "keyword");
      return {
        url: submission.uri.spec,
        postData: submission.postData,
        mayInheritPrincipal,
      };
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
      [url, postData] = await BrowserUtils.parseUrlAndPostData(
        entry.url.href,
        entry.postData,
        param
      );
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
  getPostDataStream(
    postDataString,
    type = "application/x-www-form-urlencoded"
  ) {
    let dataStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
      Ci.nsIStringInputStream
    );
    dataStream.data = postDataString;

    let mimeStream = Cc[
      "@mozilla.org/network/mime-input-stream;1"
    ].createInstance(Ci.nsIMIMEInputStream);
    mimeStream.addHeader("Content-Type", type);
    mimeStream.setData(dataStream);
    return mimeStream.QueryInterface(Ci.nsIInputStream);
  },

  _compareIgnoringDiacritics: null,

  /**
   * Returns a list of all the token substring matches in a string.  Matching is
   * case insensitive.  Each match in the returned list is a tuple: [matchIndex,
   * matchLength].  matchIndex is the index in the string of the match, and
   * matchLength is the length of the match.
   *
   * @param {array} tokens The tokens to search for.
   * @param {string} str The string to match against.
   * @param {boolean} highlightType If HIGHLIGHT.SUGGESTED, return a list of all
   *   the token string non-matches. Otherwise, return matches.
   * @returns {array} An array: [
   *            [matchIndex_0, matchLength_0],
   *            [matchIndex_1, matchLength_1],
   *            ...
   *            [matchIndex_n, matchLength_n]
   *          ].
   *          The array is sorted by match indexes ascending.
   */
  getTokenMatches(tokens, str, highlightType) {
    str = str.toLocaleLowerCase();
    // To generate non-overlapping ranges, we start from a 0-filled array with
    // the same length of the string, and use it as a collision marker, setting
    // 1 where a token matches.
    let hits = new Array(str.length).fill(
      highlightType == this.HIGHLIGHT.SUGGESTED ? 1 : 0
    );
    let compareIgnoringDiacritics;
    for (let { lowerCaseValue: needle } of tokens) {
      // Ideally we should never hit the empty token case, but just in case
      // the `needle` check protects us from an infinite loop.
      if (!needle) {
        continue;
      }
      let index = 0;
      let found = false;
      // First try a diacritic-sensitive search.
      for (;;) {
        index = str.indexOf(needle, index);
        if (index < 0) {
          break;
        }
        hits.fill(
          highlightType == this.HIGHLIGHT.SUGGESTED ? 0 : 1,
          index,
          index + needle.length
        );
        index += needle.length;
        found = true;
      }
      // If that fails to match anything, try a (computationally intensive)
      // diacritic-insensitive search.
      if (!found) {
        if (!compareIgnoringDiacritics) {
          if (!this._compareIgnoringDiacritics) {
            // Diacritic insensitivity in the search engine follows a set of
            // general rules that are not locale-dependent, so use a generic
            // English collator for highlighting matching words instead of a
            // collator for the user's particular locale.
            this._compareIgnoringDiacritics = new Intl.Collator("en", {
              sensitivity: "base",
            }).compare;
          }
          compareIgnoringDiacritics = this._compareIgnoringDiacritics;
        }
        index = 0;
        while (index < str.length) {
          let hay = str.substr(index, needle.length);
          if (compareIgnoringDiacritics(needle, hay) === 0) {
            hits.fill(
              highlightType == this.HIGHLIGHT.SUGGESTED ? 0 : 1,
              index,
              index + needle.length
            );
            index += needle.length;
          } else {
            index++;
          }
        }
      }
    }
    // Starting from the collision array, generate [start, len] tuples
    // representing the ranges to be highlighted.
    let ranges = [];
    for (let index = hits.indexOf(1); index >= 0 && index < hits.length; ) {
      let len = 0;
      // eslint-disable-next-line no-empty
      for (let j = index; j < hits.length && hits[j]; ++j, ++len) {}
      ranges.push([index, len]);
      // Move to the next 1.
      index = hits.indexOf(1, index + len);
    }
    return ranges;
  },

  /**
   * Extracts an url from a result, if possible.
   * @param {UrlbarResult} result The result to extract from.
   * @returns {object} a {url, postData} object, or null if a url can't be built
   *          from this result.
   */
  getUrlFromResult(result) {
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.URL:
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        return { url: result.payload.url, postData: null };
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
        return {
          url: result.payload.url,
          postData: result.payload.postData
            ? this.getPostDataStream(result.payload.postData)
            : null,
        };
      case UrlbarUtils.RESULT_TYPE.SEARCH: {
        const engine = Services.search.getEngineByName(result.payload.engine);
        let [url, postData] = this.getSearchQueryUrl(
          engine,
          result.payload.suggestion || result.payload.query
        );
        return { url, postData };
      }
      case UrlbarUtils.RESULT_TYPE.TIP: {
        // Return the button URL. Consumers must check payload.helpUrl
        // themselves if they need the tip's help link.
        return { url: result.payload.buttonUrl, postData: null };
      }
    }
    return { url: null, postData: null };
  },

  /**
   * Get the url to load for the search query.
   *
   * @param {nsISearchEngine} engine
   *   The engine to generate the query for.
   * @param {string} query
   *   The query string to search for.
   * @returns {array}
   *   Returns an array containing the query url (string) and the
   *    post data (object).
   */
  getSearchQueryUrl(engine, query) {
    let submission = engine.getSubmission(query, null, "keyword");
    return [submission.uri.spec, submission.postData];
  },

  /**
   * Get the number of rows a result should span in the autocomplete dropdown.
   *
   * @param {UrlbarResult} result The result being created.
   * @returns {number}
   *          The number of rows the result should span in the autocomplete
   *          dropdown.
   */
  getSpanForResult(result) {
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.URL:
      case UrlbarUtils.RESULT_TYPE.BOOKMARKS:
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
      case UrlbarUtils.RESULT_TYPE.SEARCH:
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        return 1;
      case UrlbarUtils.RESULT_TYPE.TIP:
        return 3;
    }
    return 1;
  },

  /**
   * Tries to initiate a speculative connection to a given url.
   * @param {nsISearchEngine|nsIURI|URL|string} urlOrEngine entity to initiate
   *        a speculative connection for.
   * @param {window} window the window from where the connection is initialized.
   * @note This is not infallible, if a speculative connection cannot be
   *       initialized, it will be a no-op.
   */
  setupSpeculativeConnection(urlOrEngine, window) {
    if (!UrlbarPrefs.get("speculativeConnect.enabled")) {
      return;
    }
    if (urlOrEngine instanceof Ci.nsISearchEngine) {
      try {
        urlOrEngine.speculativeConnect({
          window,
          originAttributes: window.gBrowser.contentPrincipal.originAttributes,
        });
      } catch (ex) {
        // Can't setup speculative connection for this url, just ignore it.
      }
      return;
    }

    if (urlOrEngine instanceof URL) {
      urlOrEngine = urlOrEngine.href;
    }

    try {
      let uri =
        urlOrEngine instanceof Ci.nsIURI
          ? urlOrEngine
          : Services.io.newURI(urlOrEngine);
      Services.io.speculativeConnect(
        uri,
        window.gBrowser.contentPrincipal,
        null
      );
    } catch (ex) {
      // Can't setup speculative connection for this url, just ignore it.
    }
  },

  /**
   * Used to filter out the javascript protocol from URIs, since we don't
   * support LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL for those.
   * @param {string} pasteData The data to check for javacript protocol.
   * @returns {string} The modified paste data.
   */
  stripUnsafeProtocolOnPaste(pasteData) {
    while (true) {
      let scheme = "";
      try {
        scheme = Services.io.extractScheme(pasteData);
      } catch (ex) {
        // If it throws, this is not a javascript scheme.
      }
      if (scheme != "javascript") {
        break;
      }

      pasteData = pasteData.substring(pasteData.indexOf(":") + 1);
    }
    return pasteData;
  },

  async addToInputHistory(url, input) {
    await PlacesUtils.withConnectionWrapper("addToInputHistory", db => {
      // use_count will asymptotically approach the max of 10.
      return db.executeCached(
        `
        INSERT OR REPLACE INTO moz_inputhistory
        SELECT h.id, IFNULL(i.input, :input), IFNULL(i.use_count, 0) * .9 + 1
        FROM moz_places h
        LEFT JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = :input
        WHERE url_hash = hash(:url) AND url = :url
      `,
        { url, input }
      );
    });
  },

  /**
   * Whether the passed-in input event is paste event.
   * @param {DOMEvent} event an input DOM event.
   * @returns {boolean} Whether the event is a paste event.
   */
  isPasteEvent(event) {
    return (
      event.inputType &&
      (event.inputType.startsWith("insertFromPaste") ||
        event.inputType == "insertFromYank")
    );
  },

  /**
   * Given a string, checks if it looks like a single word host, not containing
   * spaces nor dots (apart from a possible trailing one).
   * @note This matching should stay in sync with the related code in
   * URIFixup::KeywordURIFixup
   * @param {string} value
   * @returns {boolean} Whether the value looks like a single word host.
   */
  looksLikeSingleWordHost(value) {
    let str = value.trim();
    return this.REGEXP_SINGLE_WORD.test(str);
  },
};

XPCOMUtils.defineLazyGetter(UrlbarUtils.ICON, "DEFAULT", () => {
  return PlacesUtils.favicons.defaultFavicon.spec;
});

XPCOMUtils.defineLazyGetter(UrlbarUtils, "strings", () => {
  return Services.strings.createBundle(
    "chrome://global/locale/autocomplete.properties"
  );
});

/**
 * Payload JSON schemas for each result type.  Payloads are validated against
 * these schemas using JsonSchemaValidator.jsm.
 */
UrlbarUtils.RESULT_PAYLOAD_SCHEMA = {
  [UrlbarUtils.RESULT_TYPE.TAB_SWITCH]: {
    type: "object",
    required: ["url"],
    properties: {
      displayUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      title: {
        type: "string",
      },
      url: {
        type: "string",
      },
      userContextId: {
        type: "number",
      },
    },
  },
  [UrlbarUtils.RESULT_TYPE.SEARCH]: {
    type: "object",
    properties: {
      displayUrl: {
        type: "string",
      },
      engine: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      isPinned: {
        type: "boolean",
      },
      inPrivateWindow: {
        type: "boolean",
      },
      isPrivateEngine: {
        type: "boolean",
      },
      keyword: {
        type: "string",
      },
      keywordOffer: {
        type: "number", // UrlbarUtils.KEYWORD_OFFER
      },
      query: {
        type: "string",
      },
      isSearchHistory: {
        type: "boolean",
      },
      suggestion: {
        type: "string",
      },
      title: {
        type: "string",
      },
      url: {
        type: "string",
      },
    },
  },
  [UrlbarUtils.RESULT_TYPE.URL]: {
    type: "object",
    required: ["url"],
    properties: {
      displayUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      isPinned: {
        type: "boolean",
      },
      tags: {
        type: "array",
        items: {
          type: "string",
        },
      },
      title: {
        type: "string",
      },
      url: {
        type: "string",
      },
    },
  },
  [UrlbarUtils.RESULT_TYPE.KEYWORD]: {
    type: "object",
    required: ["keyword", "url"],
    properties: {
      displayUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      input: {
        type: "string",
      },
      keyword: {
        type: "string",
      },
      postData: {
        type: "string",
      },
      title: {
        type: "string",
      },
      url: {
        type: "string",
      },
    },
  },
  [UrlbarUtils.RESULT_TYPE.OMNIBOX]: {
    type: "object",
    required: ["keyword"],
    properties: {
      content: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      keyword: {
        type: "string",
      },
      title: {
        type: "string",
      },
    },
  },
  [UrlbarUtils.RESULT_TYPE.REMOTE_TAB]: {
    type: "object",
    required: ["device", "url"],
    properties: {
      device: {
        type: "string",
      },
      displayUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      title: {
        type: "string",
      },
      url: {
        type: "string",
      },
    },
  },
  [UrlbarUtils.RESULT_TYPE.TIP]: {
    type: "object",
    required: ["type"],
    properties: {
      // Prefer `buttonTextData` if your string is translated.  This is for
      // untranslated strings.
      buttonText: {
        type: "string",
      },
      // l10n { id, args }
      buttonTextData: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "array",
          },
        },
      },
      buttonUrl: {
        type: "string",
      },
      helpUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      // Prefer `text` if your string is translated.  This is for untranslated
      // strings.
      text: {
        type: "string",
      },
      // l10n { id, args }
      textData: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "array",
          },
        },
      },
      // `type` is used in the names of keys in the `urlbar.tips` keyed scalar
      // telemetry (see telemetry.rst).  If you add a new type, then you are
      // also adding new `urlbar.tips` keys and therefore need an expanded data
      // collection review.
      type: {
        type: "string",
        enum: [
          "extension",
          "intervention_clear",
          "intervention_refresh",
          "intervention_update_ask",
          "intervention_update_refresh",
          "intervention_update_restart",
          "intervention_update_web",
          "searchTip_onboard",
          "searchTip_redirect",
          "test", // for tests only
        ],
      },
    },
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
   * @param {boolean} options.isPrivate
   *   Set to true if this query was started from a private browsing window.
   * @param {number} options.maxResults
   *   The maximum number of results that will be displayed for this query.
   * @param {boolean} options.allowAutofill
   *   Whether or not to allow providers to include autofill results.
   * @param {number} options.userContextId
   *   The container id where this context was generated, if any.
   * @param {array} [options.sources]
   *   A list of acceptable UrlbarUtils.RESULT_SOURCE for the context.
   * @param {string} [options.engineName]
   *   If sources is restricting to just SEARCH, this property can be used to
   *   pick a specific search engine, by setting it to the name under which the
   *   engine is registered with the search service.
   * @param {boolean} [options.allowSearchSuggestions]
   *   Whether to allow search suggestions.  This is a veto, meaning that when
   *   false, suggestions will not be fetched, but when true, some other
   *   condition may still prohibit suggestions, like private browsing mode.
   *   Defaults to true.
   */
  constructor(options = {}) {
    this._checkRequiredOptions(options, [
      "allowAutofill",
      "isPrivate",
      "maxResults",
      "searchString",
    ]);

    if (isNaN(parseInt(options.maxResults))) {
      throw new Error(
        `Invalid maxResults property provided to UrlbarQueryContext`
      );
    }

    // Manage optional properties of options.
    for (let [prop, checkFn, defaultValue] of [
      ["providers", v => Array.isArray(v) && v.length],
      ["sources", v => Array.isArray(v) && v.length],
      ["engineName", v => typeof v == "string" && !!v.length],
      ["currentPage", v => typeof v == "string" && !!v.length],
      ["allowSearchSuggestions", v => true, true],
    ]) {
      if (prop in options) {
        if (!checkFn(options[prop])) {
          throw new Error(`Invalid value for option "${prop}"`);
        }
        this[prop] = options[prop];
      } else if (defaultValue !== undefined) {
        this[prop] = defaultValue;
      }
    }

    this.lastResultCount = 0;
    this.userContextId =
      options.userContextId ||
      Ci.nsIScriptSecurityManager.DEFAULT_USER_CONTEXT_ID;
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
        throw new Error(
          `Missing or empty ${optionName} provided to UrlbarQueryContext`
        );
      }
      this[optionName] = options[optionName];
    }
  }
}

/**
 * Base class for a muxer.
 * The muxer scope is to sort a given list of results.
 */
class UrlbarMuxer {
  /**
   * Unique name for the muxer, used by the context to sort results.
   * Not using a unique name will cause the newest registration to win.
   * @abstract
   */
  get name() {
    return "UrlbarMuxerBase";
  }

  /**
   * Sorts queryContext results in-place.
   * @param {UrlbarQueryContext} queryContext the context to sort results for.
   * @abstract
   */
  sort(queryContext) {
    throw new Error("Trying to access the base class, must be overridden");
  }
}

/**
 * Base class for a provider.
 * The provider scope is to query a datasource and return results from it.
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
   * Calls a method on the provider in a try-catch block and reports any error.
   * Unlike most other provider methods, `tryMethod` is not intended to be
   * overridden.
   *
   * @param {string} methodName The name of the method to call.
   * @param {*} args The method arguments.
   * @returns {*} The return value of the method, or undefined if the method
   *          throws an error.
   * @abstract
   */
  tryMethod(methodName, ...args) {
    try {
      return this[methodName](...args);
    } catch (ex) {
      Cu.reportError(ex);
    }
    return undefined;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   * @abstract
   */
  isActive(queryContext) {
    throw new Error("Trying to access the base class, must be overridden");
  }

  /**
   * Gets the provider's priority.  Priorities are numeric values starting at
   * zero and increasing in value.  Smaller values are lower priorities, and
   * larger values are higher priorities.  For a given query, `startQuery` is
   * called on only the active and highest-priority providers.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {number} The provider's priority for the given query.
   * @abstract
   */
  getPriority(queryContext) {
    // By default, all providers share the lowest priority.
    return 0;
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

  /**
   * Called when a result from the provider without a URL is picked, but
   * currently only for tip results.  The provider should handle the pick.
   * @param {UrlbarResult} result
   *   The result that was picked.
   * @abstract
   */
  pickResult(result) {
    throw new Error("Trying to access the base class, must be overridden");
  }

  /**
   * Called when the user starts and ends an engagement with the urlbar.
   *
   * @param {boolean} isPrivate True if the engagement is in a private context.
   * @param {string} state The state of the engagement, one of: start,
   *        engagement, abandonment, discard.
   */
  onEngagement(isPrivate, state) {}
}

/**
 * Class used to create a timer that can be manually fired, to immediately
 * invoke the callback, or canceled, as necessary.
 * Examples:
 *   let timer = new SkippableTimer();
 *   // Invokes the callback immediately without waiting for the delay.
 *   await timer.fire();
 *   // Cancel the timer, the callback won't be invoked.
 *   await timer.cancel();
 *   // Wait for the timer to have elapsed.
 *   await timer.promise;
 */
class SkippableTimer {
  /**
   * Creates a skippable timer for the given callback and time.
   * @param {object} options An object that configures the timer
   * @param {string} options.name The name of the timer, logged when necessary
   * @param {function} options.callback To be invoked when requested
   * @param {number} options.time A delay in milliseconds to wait for
   * @param {boolean} options.reportErrorOnTimeout If true and the timer times
   *                  out, an error will be logged with Cu.reportError
   * @param {logger} options.logger An optional logger
   */
  constructor({
    name = "<anonymous timer>",
    callback = null,
    time = 0,
    reportErrorOnTimeout = false,
    logger = null,
  } = {}) {
    this.name = name;
    this.logger = logger;

    let timerPromise = new Promise(resolve => {
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._timer.initWithCallback(
        () => {
          this._log(`Timed out!`, reportErrorOnTimeout);
          resolve();
        },
        time,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
      this._log(`Started`);
    });

    let firePromise = new Promise(resolve => {
      this.fire = () => {
        this._log(`Skipped`);
        resolve();
        return this.promise;
      };
    });

    this.promise = Promise.race([timerPromise, firePromise]).then(() => {
      // If we've been canceled, don't call back.
      if (this._timer && callback) {
        callback();
      }
    });
  }

  /**
   * Allows to cancel the timer and the callback won't be invoked.
   * It is not strictly necessary to await for this, the promise can just be
   * used to ensure all the internal work is complete.
   * @returns {promise} Resolved once all the cancelation work is complete.
   */
  cancel() {
    this._log(`Canceling`);
    this._timer.cancel();
    delete this._timer;
    return this.fire();
  }

  _log(msg, isError = false) {
    let line = `SkippableTimer :: ${this.name} :: ${msg}`;
    if (this.logger) {
      this.logger.debug(line);
    }
    if (isError) {
      Cu.reportError(line);
    }
  }
}
