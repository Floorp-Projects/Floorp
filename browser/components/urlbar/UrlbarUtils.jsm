/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports the UrlbarUtils singleton, which contains constants and
 * helper functions that are useful to all components of the urlbar.
 */

var EXPORTED_SYMBOLS = [
  "L10nCache",
  "SkippableTimer",
  "UrlbarMuxer",
  "UrlbarProvider",
  "UrlbarQueryContext",
  "UrlbarUtils",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  FormHistory: "resource://gre/modules/FormHistory.jsm",
  KeywordUtils: "resource://gre/modules/KeywordUtils.jsm",
  Log: "resource://gre/modules/Log.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

var UrlbarUtils = {
  // Extensions are allowed to add suggestions if they have registered a keyword
  // with the omnibox API. This is the maximum number of suggestions an extension
  // is allowed to add for a given search string using the omnibox API.
  // This value includes the heuristic result.
  MAX_OMNIBOX_RESULT_COUNT: 6,

  // Results are categorized into groups to help the muxer compose them.  See
  // UrlbarUtils.getResultGroup.  Since result groups are stored in result
  // buckets and result buckets are stored in prefs, additions and changes to
  // result groups may require adding UI migrations to BrowserGlue.  Be careful
  // about making trivial changes to existing groups, like renaming them,
  // because we don't want to make downgrades unnecessarily hard.
  RESULT_GROUP: {
    ABOUT_PAGES: "aboutPages",
    GENERAL: "general",
    GENERAL_PARENT: "generalParent",
    FORM_HISTORY: "formHistory",
    HEURISTIC_AUTOFILL: "heuristicAutofill",
    HEURISTIC_ENGINE_ALIAS: "heuristicEngineAlias",
    HEURISTIC_EXTENSION: "heuristicExtension",
    HEURISTIC_FALLBACK: "heuristicFallback",
    HEURISTIC_BOOKMARK_KEYWORD: "heuristicBookmarkKeyword",
    HEURISTIC_OMNIBOX: "heuristicOmnibox",
    HEURISTIC_PRELOADED: "heuristicPreloaded",
    HEURISTIC_SEARCH_TIP: "heuristicSearchTip",
    HEURISTIC_TEST: "heuristicTest",
    HEURISTIC_TOKEN_ALIAS_ENGINE: "heuristicTokenAliasEngine",
    INPUT_HISTORY: "inputHistory",
    OMNIBOX: "extension",
    PRELOADED: "preloaded",
    REMOTE_SUGGESTION: "remoteSuggestion",
    REMOTE_TAB: "remoteTab",
    SUGGESTED_INDEX: "suggestedIndex",
    TAIL_SUGGESTION: "tailSuggestion",
  },

  // Defines provider types.
  PROVIDER_TYPE: {
    // Should be executed immediately, because it returns heuristic results
    // that must be handed to the user asap.
    HEURISTIC: 1,
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
    // A type of result created at runtime, for example by an extension.
    DYNAMIC: 8,

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

  /**
   * Buckets used for logging telemetry to the FX_URLBAR_SELECTED_RESULT_TYPE_2
   * histogram.
   */
  SELECTED_RESULT_TYPES: {
    autofill: 0,
    bookmark: 1,
    history: 2,
    keyword: 3,
    searchengine: 4,
    searchsuggestion: 5,
    switchtab: 6,
    tag: 7,
    visiturl: 8,
    remotetab: 9,
    extension: 10,
    "preloaded-top-site": 11, // This is currently unused.
    tip: 12,
    topsite: 13,
    formhistory: 14,
    dynamic: 15,
    tabtosearch: 16,
    // n_values = 32, so you'll need to create a new histogram if you need more.
  },

  // This defines icon locations that are commonly used in the UI.
  ICON: {
    // DEFAULT is defined lazily so it doesn't eagerly initialize PlacesUtils.
    EXTENSION: "chrome://mozapps/skin/extensions/extension.svg",
    HISTORY: "chrome://browser/skin/history.svg",
    SEARCH_GLASS: "chrome://global/skin/icons/search-glass.svg",
    TIP: "chrome://global/skin/icons/lightbulb.svg",
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
    NONE: 0,
    TYPED: 1,
    SUGGESTED: 2,
  },

  // UrlbarProviderPlaces's autocomplete results store their titles and tags
  // together in their comments.  This separator is used to separate them.
  // After bug 1717511, we should stop using this old hack and store titles and
  // tags separately.  It's important that this be a character that no title
  // would ever have.  We use \x1F, the non-printable unit separator.
  TITLE_TAGS_SEPARATOR: "\x1F",

  // Regex matching single word hosts with an optional port; no spaces, auth or
  // path-like chars are admitted.
  REGEXP_SINGLE_WORD: /^[^\s@:/?#]+(:\d+)?$/,

  // Valid entry points for search mode. If adding a value here, please update
  // telemetry documentation and Scalars.yaml.
  SEARCH_MODE_ENTRY: new Set([
    "bookmarkmenu",
    "handoff",
    "keywordoffer",
    "oneoff",
    "other",
    "shortcut",
    "tabmenu",
    "tabtosearch",
    "tabtosearch_onboard",
    "topsites_newtab",
    "topsites_urlbar",
    "touchbar",
    "typed",
  ]),

  // The favicon service stores icons for URLs with the following protocols.
  PROTOCOLS_WITH_ICONS: [
    "chrome:",
    "moz-extension:",
    "about:",
    "http:",
    "https:",
    "ftp:",
  ],

  // Search mode objects corresponding to the local shortcuts in the view, in
  // order they appear.  Pref names are relative to the `browser.urlbar` branch.
  get LOCAL_SEARCH_MODES() {
    return [
      {
        source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
        restrict: UrlbarTokenizer.RESTRICT.BOOKMARK,
        icon: "chrome://browser/skin/bookmark.svg",
        pref: "shortcuts.bookmarks",
      },
      {
        source: UrlbarUtils.RESULT_SOURCE.TABS,
        restrict: UrlbarTokenizer.RESTRICT.OPENPAGE,
        icon: "chrome://browser/skin/tab.svg",
        pref: "shortcuts.tabs",
      },
      {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        restrict: UrlbarTokenizer.RESTRICT.HISTORY,
        icon: "chrome://browser/skin/history.svg",
        pref: "shortcuts.history",
      },
    ];
  },

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

    let engine = await Services.search.getEngineByAlias(keyword);
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
      [url, postData] = await KeywordUtils.parseUrlAndPostData(
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
   * @param {boolean} highlightType
   *   One of the HIGHLIGHT values:
   *     TYPED: match ranges matching the tokens; or
   *     SUGGESTED: match ranges for words not matching the tokens and the
   *                endings of words that start with a token.
   * @returns {array} An array: [
   *            [matchIndex_0, matchLength_0],
   *            [matchIndex_1, matchLength_1],
   *            ...
   *            [matchIndex_n, matchLength_n]
   *          ].
   *          The array is sorted by match indexes ascending.
   */
  getTokenMatches(tokens, str, highlightType) {
    // Only search a portion of the string, because not more than a certain
    // amount of characters are visible in the UI, matching over what is visible
    // would be expensive and pointless.
    str = str.substring(0, UrlbarUtils.MAX_TEXT_LENGTH).toLocaleLowerCase();
    // To generate non-overlapping ranges, we start from a 0-filled array with
    // the same length of the string, and use it as a collision marker, setting
    // 1 where the text should be highlighted.
    let hits = new Array(str.length).fill(
      highlightType == this.HIGHLIGHT.SUGGESTED ? 1 : 0
    );
    let compareIgnoringDiacritics;
    for (let i = 0, totalTokensLength = 0; i < tokens.length; i++) {
      const { lowerCaseValue: needle } = tokens[i];

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

        if (highlightType == UrlbarUtils.HIGHLIGHT.SUGGESTED) {
          // We de-emphasize the match only if it's preceded by a space, thus
          // it's a perfect match or the beginning of a longer word.
          let previousSpaceIndex = str.lastIndexOf(" ", index) + 1;
          if (index != previousSpaceIndex) {
            index += needle.length;
            // We found the token but we won't de-emphasize it, because it's not
            // after a word boundary.
            found = true;
            continue;
          }
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
            if (highlightType == UrlbarUtils.HIGHLIGHT.SUGGESTED) {
              let previousSpaceIndex = str.lastIndexOf(" ", index) + 1;
              if (index != previousSpaceIndex) {
                index += needle.length;
                continue;
              }
            }
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

      totalTokensLength += needle.length;
      if (totalTokensLength > UrlbarUtils.MAX_TEXT_LENGTH) {
        // Limit the number of tokens to reduce calculate time.
        break;
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
   * Returns the group for a result.
   *
   * @param {UrlbarResult} result
   *   The result.
   * @returns {UrlbarUtils.RESULT_GROUP}
   *   The reuslt's group.
   */
  getResultGroup(result) {
    if (result.group) {
      return result.group;
    }

    if (result.hasSuggestedIndex && !result.isSuggestedIndexRelativeToGroup) {
      return UrlbarUtils.RESULT_GROUP.SUGGESTED_INDEX;
    }
    if (result.heuristic) {
      switch (result.providerName) {
        case "AliasEngines":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_ENGINE_ALIAS;
        case "Autofill":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_AUTOFILL;
        case "BookmarkKeywords":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_BOOKMARK_KEYWORD;
        case "HeuristicFallback":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK;
        case "Omnibox":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_OMNIBOX;
        case "PreloadedSites":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_PRELOADED;
        case "TokenAliasEngines":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE;
        case "UrlbarProviderSearchTips":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_SEARCH_TIP;
        default:
          if (result.providerName.startsWith("TestProvider")) {
            return UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST;
          }
          break;
      }
      if (result.providerType == UrlbarUtils.PROVIDER_TYPE.EXTENSION) {
        return UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION;
      }
      Cu.reportError(
        "Returning HEURISTIC_FALLBACK for unrecognized heuristic result: " +
          result
      );
      return UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK;
    }

    switch (result.providerName) {
      case "AboutPages":
        return UrlbarUtils.RESULT_GROUP.ABOUT_PAGES;
      case "InputHistory":
        return UrlbarUtils.RESULT_GROUP.INPUT_HISTORY;
      case "PreloadedSites":
        return UrlbarUtils.RESULT_GROUP.PRELOADED;
      case "UrlbarProviderQuickSuggest":
        return UrlbarUtils.RESULT_GROUP.GENERAL_PARENT;
      default:
        break;
    }

    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        if (result.source == UrlbarUtils.RESULT_SOURCE.HISTORY) {
          return UrlbarUtils.RESULT_GROUP.FORM_HISTORY;
        }
        if (result.payload.tail) {
          return UrlbarUtils.RESULT_GROUP.TAIL_SUGGESTION;
        }
        if (result.payload.suggestion) {
          return UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION;
        }
        break;
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        return UrlbarUtils.RESULT_GROUP.OMNIBOX;
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
        return UrlbarUtils.RESULT_GROUP.REMOTE_TAB;
    }
    return UrlbarUtils.RESULT_GROUP.GENERAL;
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
        if (result.payload.engine) {
          const engine = Services.search.getEngineByName(result.payload.engine);
          let [url, postData] = this.getSearchQueryUrl(
            engine,
            result.payload.suggestion || result.payload.query
          );
          return { url, postData };
        }
        break;
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

  // Ranks a URL prefix from 3 - 0 with the following preferences:
  // https:// > https://www. > http:// > http://www.
  // Higher is better for the purposes of deduping URLs.
  // Returns -1 if the prefix does not match any of the above.
  getPrefixRank(prefix) {
    return ["http://www.", "http://", "https://www.", "https://"].indexOf(
      prefix
    );
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
    if (result.resultSpan) {
      return result.resultSpan;
    }
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
   * Gets a default icon for a URL.
   * @param {string} url
   * @returns {string} A URI pointing to an icon for `url`.
   */
  getIconForUrl(url) {
    if (typeof url == "string") {
      return UrlbarUtils.PROTOCOLS_WITH_ICONS.some(p => url.startsWith(p))
        ? "page-icon:" + url
        : UrlbarUtils.ICON.DEFAULT;
    }
    if (
      url instanceof URL &&
      UrlbarUtils.PROTOCOLS_WITH_ICONS.includes(url.protocol)
    ) {
      return "page-icon:" + url.href;
    }
    return UrlbarUtils.ICON.DEFAULT;
  },

  /**
   * Returns a search mode object if a token should enter search mode when
   * typed. This does not handle engine aliases.
   *
   * @param {UrlbarUtils.RESTRICT} token
   *   A restriction token to convert to search mode.
   * @returns {object}
   *   A search mode object. Null if search mode should not be entered. See
   *   setSearchMode documentation for details.
   */
  searchModeForToken(token) {
    if (token == UrlbarTokenizer.RESTRICT.SEARCH) {
      return {
        engineName: UrlbarSearchUtils.getDefaultEngine(this.isPrivate).name,
      };
    }

    let mode = UrlbarUtils.LOCAL_SEARCH_MODES.find(m => m.restrict == token);
    if (!mode) {
      return null;
    }

    // Return a copy so callers don't modify the object in LOCAL_SEARCH_MODES.
    return { ...mode };
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
   * Strips parts of a URL defined in `options`.
   *
   * @param {string} spec
   *        The text to modify.
   * @param {object} options
   * @param {boolean} options.stripHttp
   *        Whether to strip http.
   * @param {boolean} options.stripHttps
   *        Whether to strip https.
   * @param {boolean} options.stripWww
   *        Whether to strip `www.`.
   * @param {boolean} options.trimSlash
   *        Whether to trim the trailing slash.
   * @param {boolean} options.trimEmptyQuery
   *        Whether to trim a trailing `?`.
   * @param {boolean} options.trimEmptyHash
   *        Whether to trim a trailing `#`.
   * @param {boolean} options.trimTrailingDot
   *        Whether to trim a trailing '.'.
   * @returns {array} [modified, prefix, suffix]
   *          modified: {string} The modified spec.
   *          prefix: {string} The parts stripped from the prefix, if any.
   *          suffix: {string} The parts trimmed from the suffix, if any.
   */
  stripPrefixAndTrim(spec, options = {}) {
    let prefix = "";
    let suffix = "";
    if (options.stripHttp && spec.startsWith("http://")) {
      spec = spec.slice(7);
      prefix = "http://";
    } else if (options.stripHttps && spec.startsWith("https://")) {
      spec = spec.slice(8);
      prefix = "https://";
    }
    if (options.stripWww && spec.startsWith("www.")) {
      spec = spec.slice(4);
      prefix += "www.";
    }
    if (options.trimEmptyHash && spec.endsWith("#")) {
      spec = spec.slice(0, -1);
      suffix = "#" + suffix;
    }
    if (options.trimEmptyQuery && spec.endsWith("?")) {
      spec = spec.slice(0, -1);
      suffix = "?" + suffix;
    }
    if (options.trimSlash && spec.endsWith("/")) {
      spec = spec.slice(0, -1);
      suffix = "/" + suffix;
    }
    if (options.trimTrailingDot && spec.endsWith(".")) {
      spec = spec.slice(0, -1);
      suffix = "." + suffix;
    }
    return [spec, prefix, suffix];
  },

  /**
   * Strips a PSL verified public suffix from an hostname.
   * @param {string} host A host name.
   * @returns {string} Host name without the public suffix.
   * @note Because stripping the full suffix requires to verify it against the
   *   Public Suffix List, this call is not the cheapest, and thus it should
   *   not be used in hot paths.
   */
  stripPublicSuffixFromHost(host) {
    try {
      return host.substring(
        0,
        host.length - Services.eTLD.getKnownPublicSuffixFromHost(host).length
      );
    } catch (ex) {
      if (ex.result != Cr.NS_ERROR_HOST_IS_IP_ADDRESS) {
        throw ex;
      }
    }
    return host;
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

  /**
   * Returns the portion of a string starting at the index where another string
   * begins.
   *
   * @param   {string} sourceStr
   *          The string to search within.
   * @param   {string} targetStr
   *          The string to search for.
   * @returns {string} The substring within sourceStr starting at targetStr, or
   *          the empty string if targetStr does not occur in sourceStr.
   */
  substringAt(sourceStr, targetStr) {
    let index = sourceStr.indexOf(targetStr);
    return index < 0 ? "" : sourceStr.substr(index);
  },

  /**
   * Returns the portion of a string starting at the index where another string
   * ends.
   *
   * @param   {string} sourceStr
   *          The string to search within.
   * @param   {string} targetStr
   *          The string to search for.
   * @returns {string} The substring within sourceStr where targetStr ends, or
   *          the empty string if targetStr does not occur in sourceStr.
   */
  substringAfter(sourceStr, targetStr) {
    let index = sourceStr.indexOf(targetStr);
    return index < 0 ? "" : sourceStr.substr(index + targetStr.length);
  },

  /**
   * Strips the prefix from a URL and returns the prefix and the remainder of the
   * URL.  "Prefix" is defined to be the scheme and colon, plus, if present, two
   * slashes.  If the given string is not actually a URL, then an empty prefix and
   * the string itself is returned.
   *
   * @param {string} str The possible URL to strip.
   * @returns {array} If `str` is a URL, then [prefix, remainder].  Otherwise, ["", str].
   */
  stripURLPrefix(str) {
    let match = UrlbarTokenizer.REGEXP_PREFIX.exec(str);
    if (!match) {
      return ["", str];
    }
    let prefix = match[0];
    if (prefix.length < str.length && str[prefix.length] == " ") {
      return ["", str];
    }
    return [prefix, str.substr(prefix.length)];
  },

  /**
   * Runs a search for the given string, and returns the heuristic result.
   * @param {string} searchString The string to search for.
   * @param {nsIDOMWindow} window The window requesting it.
   * @returns {UrlbarResult} an heuristic result.
   */
  async getHeuristicResultFor(
    searchString,
    window = BrowserWindowTracker.getTopWindow()
  ) {
    if (!searchString) {
      throw new Error("Must pass a non-null search string");
    }

    let options = {
      allowAutofill: false,
      isPrivate: PrivateBrowsingUtils.isWindowPrivate(window),
      maxResults: 1,
      searchString,
      userContextId: window.gBrowser.selectedBrowser.getAttribute(
        "usercontextid"
      ),
      allowSearchSuggestions: false,
      providers: ["AliasEngines", "BookmarkKeywords", "HeuristicFallback"],
    };
    if (window.gURLBar.searchMode) {
      let searchMode = window.gURLBar.searchMode;
      options.searchMode = searchMode;
      if (searchMode.source) {
        options.sources = [searchMode.source];
      }
    }
    let context = new UrlbarQueryContext(options);
    await UrlbarProvidersManager.startQuery(context);
    if (!context.heuristicResult) {
      throw new Error("There should always be an heuristic result");
    }
    return context.heuristicResult;
  },

  /**
   * Creates a logger.
   * Logging level can be controlled through browser.urlbar.loglevel.
   * @param {string} [prefix] Prefix to use for the logged messages, "::" will
   *                 be appended automatically to the prefix.
   * @returns {object} The logger.
   */
  getLogger({ prefix = "" } = {}) {
    if (!this._logger) {
      this._logger = Log.repository.getLogger("urlbar");
      this._logger.manageLevelFromPref("browser.urlbar.loglevel");
      this._logger.addAppender(
        new Log.ConsoleAppender(new Log.BasicFormatter())
      );
    }
    if (prefix) {
      // This is not an early return because it is necessary to invoke getLogger
      // at least once before getLoggerWithMessagePrefix; it replaces a
      // method of the original logger, rather than using an actual Proxy.
      return Log.repository.getLoggerWithMessagePrefix("urlbar", prefix + "::");
    }
    return this._logger;
  },

  /**
   * Returns the name of a result source.  The name is the lowercase name of the
   * corresponding property in the RESULT_SOURCE object.
   *
   * @param {string} source A UrlbarUtils.RESULT_SOURCE value.
   * @returns {string} The token's name, a lowercased name in the RESULT_SOURCE
   *   object.
   */
  getResultSourceName(source) {
    if (!this._resultSourceNamesBySource) {
      this._resultSourceNamesBySource = new Map();
      for (let [name, src] of Object.entries(this.RESULT_SOURCE)) {
        this._resultSourceNamesBySource.set(src, name.toLowerCase());
      }
    }
    return this._resultSourceNamesBySource.get(source);
  },

  /**
   * Add the search to form history.  This also updates any existing form
   * history for the search.
   * @param {UrlbarInput} input The UrlbarInput object requesting the addition.
   * @param {string} value The value to add.
   * @param {string} [source] The source of the addition, usually
   *        the name of the engine the search was made with.
   * @returns {Promise} resolved once the operation is complete
   */
  addToFormHistory(input, value, source) {
    // If the user types a search engine alias without a search string,
    // we have an empty search string and we can't bump it.
    // We also don't want to add history in private browsing mode.
    // Finally we don't want to store extremely long strings that would not be
    // particularly useful to the user.
    if (
      !value ||
      input.isPrivate ||
      value.length > SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH
    ) {
      return Promise.resolve();
    }
    return new Promise((resolve, reject) => {
      FormHistory.update(
        {
          op: "bump",
          fieldname: input.formHistoryName,
          value,
          source,
        },
        {
          handleError: reject,
          handleCompletion: resolve,
        }
      );
    });
  },

  /**
   * Return whether the candidate can autofill to the url.
   *
   * @param {string} url
   * @param {string} candidate
   * @param {string} checkFragmentOnly
   *                 If want to check the fragment only, pass true.
   *                 Otherwise, check whole url.
   * @returns {boolean} true: can autofill
   */
  canAutofillURL(url, candidate, checkFragmentOnly = false) {
    if (
      !checkFragmentOnly &&
      (url.length <= candidate.length ||
        !url.toLocaleLowerCase().startsWith(candidate.toLocaleLowerCase()))
    ) {
      return false;
    }

    if (!UrlbarTokenizer.REGEXP_PREFIX.test(url)) {
      url = "http://" + url;
    }

    if (!UrlbarTokenizer.REGEXP_PREFIX.test(candidate)) {
      candidate = "http://" + candidate;
    }

    try {
      url = new URL(url);
      candidate = new URL(candidate);
    } catch (e) {
      return false;
    }

    if (
      !checkFragmentOnly &&
      candidate.href.endsWith("/") &&
      (url.pathname.length > candidate.pathname.length || url.hash)
    ) {
      return false;
    }

    return url.hash.startsWith(candidate.hash);
  },

  /**
   * Extracts a telemetry type from a result, used by scalars and event
   * telemetry.
   *
   * @param {UrlbarResult} result The result to analyze.
   * @returns {string} A string type for telemetry.
   * @note New types should be added to Scalars.yaml under the urlbar.picked
   *       category and documented in the in-tree documentation. A data-review
   *       is always necessary.
   */
  telemetryTypeFromResult(result) {
    if (!result) {
      return "unknown";
    }
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        return "switchtab";
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        if (result.source == UrlbarUtils.RESULT_SOURCE.HISTORY) {
          return "formhistory";
        }
        if (result.providerName == "TabToSearch") {
          return "tabtosearch";
        }
        return result.payload.suggestion ? "searchsuggestion" : "searchengine";
      case UrlbarUtils.RESULT_TYPE.URL:
        if (result.autofill) {
          return "autofill";
        }
        if (
          result.source == UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL &&
          result.heuristic
        ) {
          return "visiturl";
        }
        return result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS
          ? "bookmark"
          : "history";
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
        return "keyword";
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        return "extension";
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
        return "remotetab";
      case UrlbarUtils.RESULT_TYPE.TIP:
        return "tip";
      case UrlbarUtils.RESULT_TYPE.DYNAMIC:
        if (result.providerName == "TabToSearch") {
          // This is the onboarding result.
          return "tabtosearch";
        }
        return "dynamic";
    }
    return "unknown";
  },

  /**
   * Unescape the given uri to use as UI.
   * NOTE: If the length of uri is over MAX_TEXT_LENGTH,
   *       return the given uri as it is.
   *
   * @param {string} uri will be unescaped.
   * @returns {string} Unescaped uri.
   */
  unEscapeURIForUI(uri) {
    return uri.length > UrlbarUtils.MAX_TEXT_LENGTH
      ? uri
      : Services.textToSubURI.unEscapeURIForUI(uri);
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
      inPrivateWindow: {
        type: "boolean",
      },
      isPinned: {
        type: "boolean",
      },
      isPrivateEngine: {
        type: "boolean",
      },
      isGeneralPurposeEngine: {
        type: "boolean",
      },
      keyword: {
        type: "string",
      },
      lowerCaseSuggestion: {
        type: "string",
      },
      providesSearchMode: {
        type: "boolean",
      },
      query: {
        type: "string",
      },
      satisfiesAutofillThreshold: {
        type: "boolean",
      },
      suggestion: {
        type: "string",
      },
      tail: {
        type: "string",
      },
      tailPrefix: {
        type: "string",
      },
      tailOffsetIndex: {
        type: "number",
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
      helpL10nId: {
        type: "string",
      },
      helpUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      isPinned: {
        type: "boolean",
      },
      isSponsored: {
        type: "boolean",
      },
      qsSuggestion: {
        type: "string",
      },
      sendAttributionRequest: {
        type: "boolean",
      },
      sponsoredAdvertiser: {
        type: "string",
      },
      sponsoredBlockId: {
        type: "number",
      },
      sponsoredClickUrl: {
        type: "string",
      },
      sponsoredImpressionUrl: {
        type: "string",
      },
      sponsoredL10nId: {
        type: "string",
      },
      sponsoredTileId: {
        type: "number",
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
    required: ["device", "url", "lastUsed"],
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
      lastUsed: {
        type: "number",
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
  [UrlbarUtils.RESULT_TYPE.DYNAMIC]: {
    type: "object",
    required: ["dynamicType"],
    properties: {
      dynamicType: {
        type: "string",
      },
      // If `shouldNavigate` is `true` and the payload contains a `url`
      // property, when the result is selected the browser will navigate to the
      // `url`.
      shouldNavigate: {
        type: "boolean",
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
   * @param {object} [options.searchMode]
   *   The input's current search mode.  See UrlbarInput.setSearchMode for a
   *   description.
   * @param {boolean} [options.allowSearchSuggestions]
   *   Whether to allow search suggestions.  This is a veto, meaning that when
   *   false, suggestions will not be fetched, but when true, some other
   *   condition may still prohibit suggestions, like private browsing mode.
   *   Defaults to true.
   * @param {string} [options.formHistoryName]
   *   The name under which the local form history is registered.
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
      ["allowSearchSuggestions", v => true, true],
      ["currentPage", v => typeof v == "string" && !!v.length],
      ["formHistoryName", v => typeof v == "string" && !!v.length],
      ["providers", v => Array.isArray(v) && v.length],
      ["searchMode", v => v && typeof v == "object"],
      ["sources", v => Array.isArray(v) && v.length],
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
    // Note that Set is not serializable through JSON, so these may not be
    // easily shared with add-ons.
    this.pendingHeuristicProviders = new Set();
    this.deferUserSelectionProviders = new Set();
    this.trimmedSearchString = this.searchString.trim();
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

  /**
   * Caches and returns fixup info from URIFixup for the current search string.
   * Only returns a subset of the properties from URIFixup. This is both to
   * reduce the memory footprint of UrlbarQueryContexts and to keep them
   * serializable so they can be sent to extensions.
   */
  get fixupInfo() {
    if (this.trimmedSearchString && !this._fixupInfo) {
      let flags =
        Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
        Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
      if (this.isPrivate) {
        flags |= Ci.nsIURIFixup.FIXUP_FLAG_PRIVATE_CONTEXT;
      }

      try {
        let info = Services.uriFixup.getFixupURIInfo(
          this.trimmedSearchString,
          flags
        );
        this._fixupInfo = {
          href: info.fixedURI.spec,
          isSearch: !!info.keywordAsSent,
        };
      } catch (ex) {
        this._fixupError = ex.result;
      }
    }

    return this._fixupInfo || null;
  }

  /**
   * Returns the error that was thrown when fixupInfo was fetched, if any. If
   * fixupInfo has not yet been fetched for this queryContext, it is fetched
   * here.
   */
  get fixupError() {
    if (!this.fixupInfo) {
      return this._fixupError;
    }

    return null;
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
  constructor() {
    XPCOMUtils.defineLazyGetter(this, "logger", () =>
      UrlbarUtils.getLogger({ prefix: `Provider.${this.name}` })
    );
  }

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
    // Override this with your clean-up on cancel code.
  }

  /**
   * Called when a result from the provider is picked, but currently only for
   * tip and dynamic results.  The provider should handle the pick.  For tip
   * results, this is called only when the tip's payload doesn't have a URL.
   * For dynamic results, this is called when any selectable element in the
   * result's view is picked.
   *
   * @param {UrlbarResult} result
   *   The result that was picked.
   * @param {Element} element
   *   The element in the result's view that was picked.
   * @abstract
   */
  pickResult(result, element) {}

  /**
   * Called when the user starts and ends an engagement with the urlbar.
   *
   * @param {boolean} isPrivate
   *   True if the engagement is in a private context.
   * @param {string} state
   *   The state of the engagement, one of the following strings:
   *
   *   * start
   *       A new query has started in the urlbar.
   *   * engagement
   *       The user picked a result in the urlbar or used paste-and-go.
   *   * abandonment
   *       The urlbar was blurred (i.e., lost focus).
   *   * discard
   *       This doesn't correspond to a user action, but it means that the
   *       urlbar has discarded the engagement for some reason, and the
   *       `onEngagement` implementation should ignore it.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The engagement's query context.  This is *not* guaranteed to be defined
   *   when `state` is "start".  It will always be defined for "engagement" and
   *   "abandonment".
   * @param {object} details
   *   This is defined only when `state` is "engagement" or "abandonment", and
   *   it describes the search string and picked result.  For "engagement", it
   *   has the following properties:
   *
   *   * {string} searchString
   *       The search string for the engagement's query.
   *   * {number} selIndex
   *       The index of the picked result.
   *   * {string} selType
   *       The type of the selected result.  See TelemetryEvent.record() in
   *       UrlbarController.jsm.
   *   * {string} provider
   *       The name of the provider that produced the picked result.
   *
   *   For "abandonment", only `searchString` is defined.
   */
  onEngagement(isPrivate, state, queryContext, details) {}

  /**
   * Called when a result from the provider is selected. "Selected" refers to
   * the user highlighing the result with the arrow keys/Tab, before it is
   * picked. onSelection is also called when a user clicks a result. In the
   * event of a click, onSelection is called just before pickResult. Note that
   * this is called when heuristic results are pre-selected.
   *
   * @param {UrlbarResult} result
   *   The result that was selected.
   * @param {Element} element
   *   The element in the result's view that was selected.
   * @abstract
   */
  onSelection(result, element) {}

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update that looks like this:
   *
   *   {
   *     nodeNameFoo: {
   *       attributes: {
   *         someAttribute: someValue,
   *       },
   *       style: {
   *         someStyleProperty: someValue,
   *       },
   *       l10n: {
   *         id: someL10nId,
   *         args: someL10nArgs,
   *       },
   *       textContent: "some text content",
   *     },
   *     nodeNameBar: {
   *       ...
   *     },
   *     nodeNameBaz: {
   *       ...
   *     },
   *   }
   *
   * The object should contain a property for each element to update in the
   * dynamic result type view.  The names of these properties are the names
   * declared in the view template of the dynamic result type; see
   * UrlbarView.addDynamicViewTemplate().  The values are similar to the nested
   * objects specified in the view template but not quite the same; see below.
   * For each property, the element in the view subtree with the specified name
   * is updated according to the object in the property's value.  If an
   * element's name is not specified, then it will not be updated and will
   * retain its current state.
   *
   * @param {UrlbarResult} result
   *   The result whose view will be updated.
   * @param {Map} idsByName
   *   A Map from an element's name, as defined by the provider; to its ID in
   *   the DOM, as defined by the browser. The browser manages element IDs for
   *   dynamic results to prevent collisions. However, a provider may need to
   *   access the IDs of the elements created for its results. For example, to
   *   set various `aria` attributes.
   * @returns {object}
   *   A view update object as described above.  The names of properties are the
   *   the names of elements declared in the view template.  The values of
   *   properties are objects that describe how to update each element, and
   *   these objects may include the following properties, all of which are
   *   optional:
   *
   *   {object} [attributes]
   *     A mapping from attribute names to values.  Each name-value pair results
   *     in an attribute being added to the element.  The `id` attribute is
   *     reserved and cannot be set by the provider.
   *   {object} [style]
   *     A plain object that can be used to add inline styles to the element,
   *     like `display: none`.   `element.style` is updated for each name-value
   *     pair in this object.
   *   {object} [l10n]
   *     An { id, args } object that will be passed to
   *     document.l10n.setAttributes().
   *   {string} [textContent]
   *     A string that will be set as `element.textContent`.
   */
  getViewUpdate(result, idsByName) {
    return null;
  }

  /**
   * Defines whether the view should defer user selection events while waiting
   * for the first result from this provider.
   *
   * @returns {boolean} Whether the provider wants to defer user selection
   *          events.
   * @see UrlbarEventBufferer
   * @note UrlbarEventBufferer has a timeout after which user events will be
   *       processed regardless.
   */
  get deferUserSelection() {
    return false;
  }
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

/**
 * This class implements a cache for l10n strings. Cached strings can be
 * accessed synchronously, avoiding the asynchronicity of `data-l10n-id` and
 * `document.l10n.setAttributes`, which can lead to text pop-in and flickering
 * as strings are fetched from Fluent. (`document.l10n.formatValueSync` is also
 * sync but should not be used since it may perform sync I/O.)
 *
 * Values stored and returned by the cache are JS objects similar to
 * `L10nMessage` objects, not bare strings. This allows the cache to store not
 * only l10n strings with bare values but also strings that define attributes
 * (e.g., ".label = My label value"). See `get` for details.
 */
class L10nCache {
  /**
   * @param {Localization} l10n
   *   A `Localization` object like `document.l10n`. This class keeps a weak
   *   reference to this object, so the caller or something else must hold onto
   *   it.
   */
  constructor(l10n) {
    this.l10n = Cu.getWeakReference(l10n);
  }

  /**
   * Gets a cached l10n message.
   *
   * @param {string} id
   *   The string's Fluent ID.
   * @param {object} [args]
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   * @returns {object}
   *   The message object or undefined if it's not cached. The message object is
   *   similar to `L10nMessage` (defined in Localization.webidl) but its
   *   attributes are stored differently for convenience. It looks like this:
   *
   *     { value, attributes }
   *
   *   The properties are:
   *
   *     {string} value
   *       The bare value of the string. If the string does not have a bare
   *       value (i.e., it has only attributes), this will be null.
   *     {object} attributes
   *       A mapping from attribute names to their values. If the string doesn't
   *       have any attributes, this will be null.
   *
   *   For example, if we cache these strings from an ftl file:
   *
   *     foo = Foo's value
   *     bar =
   *       .label = Bar's label value
   *
   *   Then:
   *
   *     cache.get("foo")
   *     // => { value: "Foo's value", attributes: null }
   *     cache.get("bar")
   *     // => { value: null, attributes: { label: "Bar's label value" }}
   */
  get(id, args = undefined) {
    return this._messagesByKey.get(this._key(id, args));
  }

  /**
   * Fetches a string from Fluent and caches it.
   *
   * @param {string} id
   *   The string's Fluent ID.
   * @param {object} [args]
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   */
  async add(id, args = undefined) {
    let l10n = this.l10n.get();
    if (!l10n) {
      return;
    }
    let messages = await l10n.formatMessages([{ id, args }]);
    if (!messages?.length) {
      Cu.reportError(
        "l10n.formatMessages returned an unexpected value for ID: " + id
      );
      return;
    }
    let message = messages[0];
    if (message.attributes) {
      // Convert `attributes` from an array of `{ name, value }` objects to one
      // object mapping names to values.
      message.attributes = message.attributes.reduce(
        (valuesByName, { name, value }) => {
          valuesByName[name] = value;
          return valuesByName;
        },
        {}
      );
    }
    this._messagesByKey.set(this._key(id, args), message);
  }

  /**
   * Fetches and caches a string if it's not already cached. This is just a
   * slight optimization over `add` that avoids calling into Fluent
   * unnecessarily.
   *
   * @param {string} id
   *   The string's Fluent ID.
   * @param {object} [args]
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   */
  async ensure(id, args = undefined) {
    if (!this.get(id, args)) {
      await this.add(id, args);
    }
  }

  /**
   * Fetches and caches strings that aren't already cached.
   *
   * @param {array} idArgs
   *   An array of `{ id, args }` objects.
   */
  async ensureAll(idArgs) {
    let promises = [];
    for (let { id, args } of idArgs) {
      promises.push(this.ensure(id, args));
    }
    await Promise.all(promises);
  }

  /**
   * Removes a cached string.
   *
   * @param {string} id
   *   The string's Fluent ID.
   * @param {object} [args]
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   */
  delete(id, args = undefined) {
    this._messagesByKey.delete(this._key(id, args));
  }

  /**
   * Removes all cached strings.
   */
  clear() {
    this._messagesByKey.clear();
  }

  /**
   * Cache keys => cached message objects
   */
  _messagesByKey = new Map();

  /**
   * Returns a cache key for a string in `_messagesByKey`.
   *
   * @param {string} id
   *   The string's Fluent ID.
   * @param {object} [args]
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   * @returns {string}
   *   The cache key.
   */
  _key(id, args) {
    // Keys are `id` plus JSON'ed `args` values. `JSON.stringify` doesn't
    // guarantee a particular ordering of object properties, so instead of
    // stringifying `args` as is, sort its entries by key and then pull out the
    // values. The final key is a JSON'ed array of `id` concatenated with the
    // sorted-by-key `args` values.
    let argValues = Object.entries(args || [])
      .sort(([key1], [key2]) => key1.localeCompare(key2))
      .map(([_, value]) => value);
    let parts = [id].concat(argValues);
    return JSON.stringify(parts);
  }
}
