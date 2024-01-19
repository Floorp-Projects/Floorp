/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports the UrlbarUtils singleton, which contains constants and
 * helper functions that are useful to all components of the urlbar.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  KeywordUtils: "resource://gre/modules/KeywordUtils.sys.mjs",
  Log: "resource://gre/modules/Log.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SearchSuggestionController:
    "resource://gre/modules/SearchSuggestionController.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarProviderSearchTips:
    "resource:///modules/UrlbarProviderSearchTips.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
  BrowserUIUtils: "resource:///modules/BrowserUIUtils.sys.mjs",
});

export var UrlbarUtils = {
  // Results are categorized into groups to help the muxer compose them.  See
  // UrlbarUtils.getResultGroup.  Since result groups are stored in result
  // groups and result groups are stored in prefs, additions and changes to
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
    HEURISTIC_HISTORY_URL: "heuristicHistoryUrl",
    HEURISTIC_OMNIBOX: "heuristicOmnibox",
    HEURISTIC_SEARCH_TIP: "heuristicSearchTip",
    HEURISTIC_TEST: "heuristicTest",
    HEURISTIC_TOKEN_ALIAS_ENGINE: "heuristicTokenAliasEngine",
    INPUT_HISTORY: "inputHistory",
    OMNIBOX: "extension",
    RECENT_SEARCH: "recentSearch",
    REMOTE_SUGGESTION: "remoteSuggestion",
    REMOTE_TAB: "remoteTab",
    SUGGESTED_INDEX: "suggestedIndex",
    TAIL_SUGGESTION: "tailSuggestion",
  },

  // Defines provider types.
  PROVIDER_TYPE: {
    // Should be executed immediately, because it returns heuristic results
    // that must be handed to the user asap.
    // WARNING: these providers must be extremely fast, because the urlbar will
    // await for them before returning results to the user. In particular it is
    // critical to reply quickly to isActive and startQuery.
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
    ACTIONS: 7,
    ADDON: 8,
  },

  // This defines icon locations that are commonly used in the UI.
  ICON: {
    // DEFAULT is defined lazily so it doesn't eagerly initialize PlacesUtils.
    EXTENSION: "chrome://mozapps/skin/extensions/extension.svg",
    HISTORY: "chrome://browser/skin/history.svg",
    SEARCH_GLASS: "chrome://global/skin/icons/search-glass.svg",
    TRENDING: "chrome://global/skin/icons/trending.svg",
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
    "historymenu",
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

  // Valid URI schemes that are considered safe but don't contain
  // an authority component (e.g host:port). There are many URI schemes
  // that do not contain an authority, but these in particular have
  // some likelihood of being entered or bookmarked by a user.
  // `file:` is an exceptional case because an authority is optional
  PROTOCOLS_WITHOUT_AUTHORITY: [
    "about:",
    "data:",
    "file:",
    "javascript:",
    "view-source:",
  ],

  // Search mode objects corresponding to the local shortcuts in the view, in
  // order they appear.  Pref names are relative to the `browser.urlbar` branch.
  get LOCAL_SEARCH_MODES() {
    return [
      {
        source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
        restrict: lazy.UrlbarTokenizer.RESTRICT.BOOKMARK,
        icon: "chrome://browser/skin/bookmark.svg",
        pref: "shortcuts.bookmarks",
        telemetryLabel: "bookmarks",
      },
      {
        source: UrlbarUtils.RESULT_SOURCE.TABS,
        restrict: lazy.UrlbarTokenizer.RESTRICT.OPENPAGE,
        icon: "chrome://browser/skin/tab.svg",
        pref: "shortcuts.tabs",
        telemetryLabel: "tabs",
      },
      {
        source: UrlbarUtils.RESULT_SOURCE.HISTORY,
        restrict: lazy.UrlbarTokenizer.RESTRICT.HISTORY,
        icon: "chrome://browser/skin/history.svg",
        pref: "shortcuts.history",
        telemetryLabel: "history",
      },
      {
        source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
        restrict: lazy.UrlbarTokenizer.RESTRICT.ACTION,
        icon: "chrome://browser/skin/quickactions.svg",
        pref: "shortcuts.quickactions",
        telemetryLabel: "actions",
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
      !lazy.PrivateBrowsingUtils.isWindowPrivate(window) &&
      url &&
      !url.includes(" ") &&
      // eslint-disable-next-line no-control-regex
      !/[\x00-\x1F]/.test(url)
    ) {
      lazy.PlacesUIUtils.markPageAsTyped(url);
    }
  },

  /**
   * Given a string, will generate a more appropriate urlbar value if a Places
   * keyword or a search alias is found at the beginning of it.
   *
   * @param {string} url
   *        A string that may begin with a keyword or an alias.
   *
   * @returns {Promise<{ url, postData, mayInheritPrincipal }>}
   *        If it's not possible to discern a keyword or an alias, url will be
   *        the input string.
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
      entry = await lazy.PlacesUtils.keywords.fetch(keyword);
    } catch (ex) {
      console.error(`Unable to fetch Places keyword "${keyword}":`, ex);
    }
    if (!entry || !entry.url) {
      // This is not a Places keyword.
      return { url, postData, mayInheritPrincipal };
    }

    try {
      [url, postData] = await lazy.KeywordUtils.parseUrlAndPostData(
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
   * @param {Array} tokens The tokens to search for.
   * @param {string} str The string to match against.
   * @param {boolean} highlightType
   *   One of the HIGHLIGHT values:
   *     TYPED: match ranges matching the tokens; or
   *     SUGGESTED: match ranges for words not matching the tokens and the
   *                endings of words that start with a token.
   * @returns {Array} An array: [
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
        case "TokenAliasEngines":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_TOKEN_ALIAS_ENGINE;
        case "UrlbarProviderSearchTips":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_SEARCH_TIP;
        case "HistoryUrlHeuristic":
          return UrlbarUtils.RESULT_GROUP.HEURISTIC_HISTORY_URL;
        default:
          if (result.providerName.startsWith("TestProvider")) {
            return UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST;
          }
          break;
      }
      if (result.providerType == UrlbarUtils.PROVIDER_TYPE.EXTENSION) {
        return UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION;
      }
      console.error(
        "Returning HEURISTIC_FALLBACK for unrecognized heuristic result: ",
        result
      );
      return UrlbarUtils.RESULT_GROUP.HEURISTIC_FALLBACK;
    }

    switch (result.providerName) {
      case "AboutPages":
        return UrlbarUtils.RESULT_GROUP.ABOUT_PAGES;
      case "InputHistory":
        return UrlbarUtils.RESULT_GROUP.INPUT_HISTORY;
      case "UrlbarProviderQuickSuggest":
        return UrlbarUtils.RESULT_GROUP.GENERAL_PARENT;
      default:
        break;
    }

    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        if (result.source == UrlbarUtils.RESULT_SOURCE.HISTORY) {
          return result.providerName == "RecentSearches"
            ? UrlbarUtils.RESULT_GROUP.RECENT_SEARCH
            : UrlbarUtils.RESULT_GROUP.FORM_HISTORY;
        }
        if (result.payload.tail && !result.isRichSuggestion) {
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
   *
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
   * @returns {Array}
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

    // We know this result will be hidden in the final view so assign it
    // a span of zero.
    if (result.exposureResultHidden) {
      return 0;
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
   *
   * @param {string} url
   *   The URL to get the icon for.
   * @returns {string} A URI pointing to an icon for `url`.
   */
  getIconForUrl(url) {
    if (typeof url == "string") {
      return UrlbarUtils.PROTOCOLS_WITH_ICONS.some(p => url.startsWith(p))
        ? "page-icon:" + url
        : UrlbarUtils.ICON.DEFAULT;
    }
    if (
      URL.isInstance(url) &&
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
    if (token == lazy.UrlbarTokenizer.RESTRICT.SEARCH) {
      return {
        engineName: lazy.UrlbarSearchUtils.getDefaultEngine(this.isPrivate)
          ?.name,
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
   *
   * Note: This is not infallible, if a speculative connection cannot be
   *       initialized, it will be a no-op.
   *
   * @param {nsISearchEngine|nsIURI|URL|string} urlOrEngine entity to initiate
   *        a speculative connection for.
   * @param {window} window the window from where the connection is initialized.
   */
  setupSpeculativeConnection(urlOrEngine, window) {
    if (!lazy.UrlbarPrefs.get("speculativeConnect.enabled")) {
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

    if (URL.isInstance(urlOrEngine)) {
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
        null,
        false
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
   * @param {object} [options]
   *        The options object.
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
   * @returns {string[]} [modified, prefix, suffix]
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
   *
   * Note: Because stripping the full suffix requires to verify it against the
   *   Public Suffix List, this call is not the cheapest, and thus it should
   *   not be used in hot paths.
   *
   * @param {string} host A host name.
   * @returns {string} Host name without the public suffix.
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
   *
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

  /**
   * Add a (url, input) tuple to the input history table that drives adaptive
   * results.
   *
   * @param {string} url The url to add input history for
   * @param {string} input The associated search term
   */
  async addToInputHistory(url, input) {
    await lazy.PlacesUtils.withConnectionWrapper("addToInputHistory", db => {
      // use_count will asymptotically approach the max of 10.
      return db.executeCached(
        `
        INSERT OR REPLACE INTO moz_inputhistory
        SELECT h.id, IFNULL(i.input, :input), IFNULL(i.use_count, 0) * .9 + 1
        FROM moz_places h
        LEFT JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = :input
        WHERE url_hash = hash(:url) AND url = :url
      `,
        { url, input: input.toLowerCase() }
      );
    });
  },

  /**
   * Remove a (url, input*) tuple from the input history table that drives
   * adaptive results.
   * Note the input argument is used as a wildcard so any match starting with
   * it will also be removed.
   *
   * @param {string} url The url to add input history for
   * @param {string} input The associated search term
   */
  async removeInputHistory(url, input) {
    await lazy.PlacesUtils.withConnectionWrapper("removeInputHistory", db => {
      return db.executeCached(
        `
        DELETE FROM moz_inputhistory
        WHERE input BETWEEN :input AND :input || X'FFFF'
          AND place_id =
            (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url)
        `,
        { url, input: input.toLowerCase() }
      );
    });
  },

  /**
   * Whether the passed-in input event is paste event.
   *
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
   *
   * Note: This matching should stay in sync with the related code in
   * URIFixup::KeywordURIFixup
   *
   * @param {string} value
   *   The string to check.
   * @returns {boolean}
   *   Whether the value looks like a single word host.
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
   * Strips the prefix from a URL and returns the prefix and the remainder of
   * the URL. "Prefix" is defined to be the scheme and colon plus zero to two
   * slashes (see `UrlbarTokenizer.REGEXP_PREFIX`). If the given string is not
   * actually a URL or it has a prefix we don't recognize, then an empty prefix
   * and the string itself is returned.
   *
   * @param   {string} str The possible URL to strip.
   * @returns {Array} If `str` is a URL with a prefix we recognize,
   *          then [prefix, remainder].  Otherwise, ["", str].
   */
  stripURLPrefix(str) {
    let match = lazy.UrlbarTokenizer.REGEXP_PREFIX.exec(str);
    if (!match) {
      return ["", str];
    }
    let prefix = match[0];
    if (prefix.length < str.length && str[prefix.length] == " ") {
      // A space following a prefix:
      // e.g. "http:// some search string", "about: some search string"
      return ["", str];
    }
    if (
      prefix.endsWith(":") &&
      !UrlbarUtils.PROTOCOLS_WITHOUT_AUTHORITY.includes(prefix.toLowerCase())
    ) {
      // Something that looks like a URI scheme but we won't treat as one:
      // e.g. "localhost:8888"
      return ["", str];
    }
    return [prefix, str.substring(prefix.length)];
  },

  /**
   * Runs a search for the given string, and returns the heuristic result.
   *
   * @param {string} searchString The string to search for.
   * @param {nsIDOMWindow} window The window requesting it.
   * @returns {UrlbarResult} an heuristic result.
   */
  async getHeuristicResultFor(searchString, window) {
    if (!searchString) {
      throw new Error("Must pass a non-null search string");
    }

    let options = {
      allowAutofill: false,
      isPrivate: lazy.PrivateBrowsingUtils.isWindowPrivate(window),
      maxResults: 1,
      searchString,
      userContextId:
        window.gBrowser.selectedBrowser.getAttribute("usercontextid"),
      prohibitRemoteResults: true,
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
    await lazy.UrlbarProvidersManager.startQuery(context);
    if (!context.heuristicResult) {
      throw new Error("There should always be an heuristic result");
    }
    return context.heuristicResult;
  },

  /**
   * Creates a logger.
   * Logging level can be controlled through browser.urlbar.loglevel.
   *
   * @param {string} [prefix] Prefix to use for the logged messages, "::" will
   *                 be appended automatically to the prefix.
   * @returns {object} The logger.
   */
  getLogger({ prefix = "" } = {}) {
    if (!this._logger) {
      this._logger = lazy.Log.repository.getLogger("urlbar");
      this._logger.manageLevelFromPref("browser.urlbar.loglevel");
      this._logger.addAppender(
        new lazy.Log.ConsoleAppender(new lazy.Log.BasicFormatter())
      );
    }
    if (prefix) {
      // This is not an early return because it is necessary to invoke getLogger
      // at least once before getLoggerWithMessagePrefix; it replaces a
      // method of the original logger, rather than using an actual Proxy.
      return lazy.Log.repository.getLoggerWithMessagePrefix(
        "urlbar",
        prefix + " :: "
      );
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
   *
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
      value.length >
        lazy.SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH
    ) {
      return Promise.resolve();
    }
    return lazy.FormHistory.update({
      op: "bump",
      fieldname: input.formHistoryName,
      value,
      source,
    });
  },

  /**
   * Returns whether a URL can be autofilled from a candidate string. This
   * function is specifically designed for origin and up-to-the-next-slash URL
   * autofill. It should not be used for other types of autofill.
   *
   * @param {string} url
   *                 The URL to test
   * @param {string} candidate
   *                 The candidate string to test against
   * @param {string} checkFragmentOnly
   *                 If want to check the fragment only, pass true.
   *                 Otherwise, check whole url.
   * @returns {boolean} true: can autofill
   */
  canAutofillURL(url, candidate, checkFragmentOnly = false) {
    // If the URL does not start with the candidate, it can't be autofilled.
    // The length check is an optimization to short-circuit the `startsWith()`.
    if (
      !checkFragmentOnly &&
      (url.length <= candidate.length ||
        !url.toLocaleLowerCase().startsWith(candidate.toLocaleLowerCase()))
    ) {
      return false;
    }

    // Create `URL` objects to make the logic below easier. The strings must
    // include schemes for this to work.
    if (!lazy.UrlbarTokenizer.REGEXP_PREFIX.test(url)) {
      url = "http://" + url;
    }
    if (!lazy.UrlbarTokenizer.REGEXP_PREFIX.test(candidate)) {
      candidate = "http://" + candidate;
    }
    try {
      url = new URL(url);
      candidate = new URL(candidate);
    } catch (e) {
      return false;
    }

    if (checkFragmentOnly) {
      return url.hash.startsWith(candidate.hash);
    }

    // For both origin and URL autofill, autofill should stop when the user
    // types a trailing slash. This is a fundamental part of autofill's
    // up-to-the-next-slash behavior. We handle that here in the else-if branch.
    // The length and hash checks in the else-if condition aren't strictly
    // necessary -- the else-if branch could simply be an else-branch that
    // returns false -- but they mean this function will return true when the
    // URL and candidate have the same case-insenstive path and no hash. In
    // other words, we allow a URL to autofill itself.
    if (!candidate.href.endsWith("/")) {
      // The candidate doesn't end in a slash. The URL can't be autofilled if
      // its next slash is not at the end.
      let nextSlashIndex = url.pathname.indexOf("/", candidate.pathname.length);
      if (nextSlashIndex >= 0 && nextSlashIndex != url.pathname.length - 1) {
        return false;
      }
    } else if (url.pathname.length > candidate.pathname.length || url.hash) {
      return false;
    }

    return url.hash.startsWith(candidate.hash);
  },

  /**
   * Extracts a telemetry type from a result, used by scalars and event
   * telemetry.
   *
   * Note: New types should be added to Scalars.yaml under the urlbar.picked
   *       category and documented in the in-tree documentation. A data-review
   *       is always necessary.
   *
   * @param {UrlbarResult} result The result to analyze.
   * @returns {string} A string type for telemetry.
   */
  telemetryTypeFromResult(result) {
    if (!result) {
      return "unknown";
    }
    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        return "switchtab";
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        if (result.providerName == "RecentSearches") {
          return "recent_search";
        }
        if (result.source == UrlbarUtils.RESULT_SOURCE.HISTORY) {
          return "formhistory";
        }
        if (result.providerName == "TabToSearch") {
          return "tabtosearch";
        }
        if (result.payload.suggestion) {
          let type = result.payload.trending ? "trending" : "searchsuggestion";
          if (result.isRichSuggestion) {
            type += "_rich";
          }
          return type;
        }
        return "searchengine";
      case UrlbarUtils.RESULT_TYPE.URL:
        if (result.autofill) {
          let { type } = result.autofill;
          if (!type) {
            type = "other";
            console.error(
              new Error(
                "`result.autofill.type` not set, falling back to 'other'"
              )
            );
          }
          return `autofill_${type}`;
        }
        if (
          result.source == UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL &&
          result.heuristic
        ) {
          return "visiturl";
        }
        if (result.providerName == "UrlbarProviderQuickSuggest") {
          // Don't add any more `urlbar.picked` legacy telemetry if possible!
          // Return "quicksuggest" here and rely on Glean instead.
          switch (result.payload.telemetryType) {
            case "top_picks":
              return "navigational";
            case "wikipedia":
              return "dynamic_wikipedia";
          }
          return "quicksuggest";
        }
        if (result.providerName == "UrlbarProviderClipboard") {
          return "clipboard";
        }
        if (result.providerName == "InputHistory") {
          return result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS
            ? "bookmark_adaptive"
            : "history_adaptive";
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
        } else if (result.providerName == "quickactions") {
          return "quickaction";
        } else if (result.providerName == "Weather") {
          return "weather";
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

  /**
   * Checks whether a given text has right-to-left direction or not.
   *
   * @param {string} value The text which should be check for RTL direction.
   * @param {Window} window The window where 'value' is going to be displayed.
   * @returns {boolean} Returns true if text has right-to-left direction and
   *                    false otherwise.
   */
  isTextDirectionRTL(value, window) {
    let directionality = window.windowUtils.getDirectionFromText(value);
    return directionality == window.windowUtils.DIRECTION_RTL;
  },

  /**
   * Unescape, decode punycode, and trim (both protocol and trailing slash)
   * the URL. Use for displaying purposes only!
   *
   * @param {string} url The url that should be prepared for display.
   * @param {object} [options] Preparation options.
   * @param {boolean} [options.trimURL] Whether the displayed URL should be
   *                  trimmed or not.
   * @returns {string} Prepared url.
   */
  prepareUrlForDisplay(url, { trimURL = true } = {}) {
    // Some domains are encoded in punycode. The following ensures we display
    // the url in utf-8.
    try {
      url = new URL(url).URI.displaySpec;
    } catch {} // In some cases url is not a valid url.

    if (url && trimURL && lazy.UrlbarPrefs.get("trimURLs")) {
      url = lazy.BrowserUIUtils.removeSingleTrailingSlashFromURL(url);
      if (url.startsWith("https://")) {
        url = url.substring(8);
        if (url.startsWith("www.")) {
          url = url.substring(4);
        }
      }
    }

    return this.unEscapeURIForUI(url);
  },

  /**
   * Extracts a group for search engagement telemetry from a result.
   *
   * @param {UrlbarResult} result The result to analyze.
   * @returns {string} Group name as string.
   */
  searchEngagementTelemetryGroup(result) {
    if (!result) {
      return "unknown";
    }
    if (result.isBestMatch) {
      return "top_pick";
    }
    if (result.providerName === "UrlbarProviderTopSites") {
      return "top_site";
    }

    switch (this.getResultGroup(result)) {
      case UrlbarUtils.RESULT_GROUP.INPUT_HISTORY: {
        return "adaptive_history";
      }
      case UrlbarUtils.RESULT_GROUP.RECENT_SEARCH: {
        return "recent_search";
      }
      case UrlbarUtils.RESULT_GROUP.FORM_HISTORY: {
        return "search_history";
      }
      case UrlbarUtils.RESULT_GROUP.TAIL_SUGGESTION:
      case UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION: {
        let group = result.payload.trending
          ? "trending_search"
          : "search_suggest";
        if (result.isRichSuggestion) {
          group += "_rich";
        }
        return group;
      }
      case UrlbarUtils.RESULT_GROUP.REMOTE_TAB: {
        return "remote_tab";
      }
      case UrlbarUtils.RESULT_GROUP.HEURISTIC_EXTENSION:
      case UrlbarUtils.RESULT_GROUP.HEURISTIC_OMNIBOX:
      case UrlbarUtils.RESULT_GROUP.OMNIBOX: {
        return "addon";
      }
      case UrlbarUtils.RESULT_GROUP.GENERAL: {
        return "general";
      }
      // Group of UrlbarProviderQuickSuggest is GENERAL_PARENT.
      case UrlbarUtils.RESULT_GROUP.GENERAL_PARENT: {
        return "suggest";
      }
      case UrlbarUtils.RESULT_GROUP.ABOUT_PAGES: {
        return "about_page";
      }
      case UrlbarUtils.RESULT_GROUP.SUGGESTED_INDEX: {
        return "suggested_index";
      }
    }

    return result.heuristic ? "heuristic" : "unknown";
  },

  /**
   * Extracts a type for search engagement telemetry from a result.
   *
   * @param {UrlbarResult} result The result to analyze.
   * @param {string} selType An optional parameter for the selected type.
   * @returns {string} Type as string.
   */
  searchEngagementTelemetryType(result, selType = null) {
    if (!result) {
      return selType === "oneoff" ? "search_shortcut_button" : "input_field";
    }

    if (
      result.providerType === UrlbarUtils.PROVIDER_TYPE.EXTENSION &&
      result.providerName != "Omnibox"
    ) {
      return "experimental_addon";
    }

    switch (result.type) {
      case UrlbarUtils.RESULT_TYPE.DYNAMIC:
        switch (result.providerName) {
          case "calculator":
            return "calc";
          case "quickactions":
            return "action";
          case "TabToSearch":
            return "tab_to_search";
          case "UnitConversion":
            return "unit";
          case "UrlbarProviderContextualSearch":
            return "site_specific_contextual_search";
          case "UrlbarProviderQuickSuggest":
            return this._getQuickSuggestTelemetryType(result);
          case "UrlbarProviderQuickSuggestContextualOptIn":
            return "fxsuggest_data_sharing_opt_in";
          case "Weather":
            return "weather";
        }
        break;
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
        return "keyword";
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
        return "addon";
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
        return "remote_tab";
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        if (result.providerName === "TabToSearch") {
          return "tab_to_search";
        }
        if (result.source == UrlbarUtils.RESULT_SOURCE.HISTORY) {
          return result.providerName == "RecentSearches"
            ? "recent_search"
            : "search_history";
        }
        if (result.payload.suggestion) {
          let type = result.payload.trending
            ? "trending_search"
            : "search_suggest";
          if (result.isRichSuggestion) {
            type += "_rich";
          }
          return type;
        }
        return "search_engine";
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
        return "tab";
      case UrlbarUtils.RESULT_TYPE.TIP:
        if (result.providerName === "UrlbarProviderInterventions") {
          switch (result.payload.type) {
            case lazy.UrlbarProviderInterventions.TIP_TYPE.CLEAR:
              return "intervention_clear";
            case lazy.UrlbarProviderInterventions.TIP_TYPE.REFRESH:
              return "intervention_refresh";
            case lazy.UrlbarProviderInterventions.TIP_TYPE.UPDATE_ASK:
            case lazy.UrlbarProviderInterventions.TIP_TYPE.UPDATE_CHECKING:
            case lazy.UrlbarProviderInterventions.TIP_TYPE.UPDATE_REFRESH:
            case lazy.UrlbarProviderInterventions.TIP_TYPE.UPDATE_RESTART:
            case lazy.UrlbarProviderInterventions.TIP_TYPE.UPDATE_WEB:
              return "intervention_update";
            default:
              return "intervention_unknown";
          }
        }

        switch (result.payload.type) {
          case lazy.UrlbarProviderSearchTips.TIP_TYPE.ONBOARD:
            return "tip_onboard";
          case lazy.UrlbarProviderSearchTips.TIP_TYPE.PERSIST:
            return "tip_persist";
          case lazy.UrlbarProviderSearchTips.TIP_TYPE.REDIRECT:
            return "tip_redirect";
          case "dismissalAcknowledgment":
            return "tip_dismissal_acknowledgment";
          default:
            return "tip_unknown";
        }
      case UrlbarUtils.RESULT_TYPE.URL:
        if (
          result.source === UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL &&
          result.heuristic
        ) {
          return "url";
        }
        if (result.autofill) {
          return `autofill_${result.autofill.type ?? "unknown"}`;
        }
        if (result.providerName === "UrlbarProviderQuickSuggest") {
          return this._getQuickSuggestTelemetryType(result);
        }
        if (result.providerName === "UrlbarProviderTopSites") {
          return "top_site";
        }
        if (result.providerName === "UrlbarProviderClipboard") {
          return "clipboard";
        }
        return result.source === UrlbarUtils.RESULT_SOURCE.BOOKMARKS
          ? "bookmark"
          : "history";
    }

    return "unknown";
  },

  /**
   * Extracts a subtype for search engagement telemetry from a result and the picked element.
   *
   * @param {UrlbarResult} result The result to analyze.
   * @param {DOMElement} element The picked view element. Nullable.
   * @returns {string} Subtype as string.
   */
  searchEngagementTelemetrySubtype(result, element) {
    if (!result) {
      return "";
    }

    if (
      result.providerName === "quickactions" &&
      element?.classList.contains("urlbarView-quickaction-button")
    ) {
      return element.dataset.key;
    }

    return "";
  },

  _getQuickSuggestTelemetryType(result) {
    let source = result.payload.source;
    if (source == "remote-settings") {
      source = "rs";
    }
    return `${source}_${result.payload.telemetryType}`;
  },

  /**
   * For use when we want to hash a pair of items in a dictionary
   *
   * @param {string[]} tokens
   *   list of tokens to join into a string eg "a" "b" "c"
   * @returns {string}
   *   the tokens joined in a string "a|b|c"
   */
  tupleString(...tokens) {
    return tokens.filter(t => t).join("|");
  },
};

ChromeUtils.defineLazyGetter(UrlbarUtils.ICON, "DEFAULT", () => {
  return lazy.PlacesUtils.favicons.defaultFavicon.spec;
});

ChromeUtils.defineLazyGetter(UrlbarUtils, "strings", () => {
  return Services.strings.createBundle(
    "chrome://global/locale/autocomplete.properties"
  );
});

/**
 * Payload JSON schemas for each result type.  Payloads are validated against
 * these schemas using JsonSchemaValidator.sys.mjs.
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
      blockL10n: {
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
      description: {
        type: "string",
      },
      displayUrl: {
        type: "string",
      },
      engine: {
        type: "string",
      },
      helpUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      inPrivateWindow: {
        type: "boolean",
      },
      isBlockable: {
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
      trending: {
        type: "boolean",
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
      // l10n { id, args }
      blockL10n: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "object",
            additionalProperties: true,
          },
        },
      },
      // l10n { id, args }
      bottomTextL10n: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "object",
            additionalProperties: true,
          },
        },
      },
      description: {
        type: "string",
      },
      // l10n { id, args }
      descriptionL10n: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "object",
            additionalProperties: true,
          },
        },
      },
      displayUrl: {
        type: "string",
      },
      dupedHeuristic: {
        type: "boolean",
      },
      fallbackTitle: {
        type: "string",
      },
      // l10n { id, args }
      helpL10n: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "object",
            additionalProperties: true,
          },
        },
      },
      helpUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      iconBlob: {
        type: "object",
      },
      isBlockable: {
        type: "boolean",
      },
      isPinned: {
        type: "boolean",
      },
      isSponsored: {
        type: "boolean",
      },
      originalUrl: {
        type: "string",
      },
      provider: {
        type: "string",
      },
      qsSuggestion: {
        type: "string",
      },
      requestId: {
        type: "string",
      },
      sendAttributionRequest: {
        type: "boolean",
      },
      shouldShowUrl: {
        type: "boolean",
      },
      source: {
        type: "string",
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
      sponsoredIabCategory: {
        type: "string",
      },
      sponsoredImpressionUrl: {
        type: "string",
      },
      sponsoredTileId: {
        type: "number",
      },
      subtype: {
        type: "string",
      },
      tags: {
        type: "array",
        items: {
          type: "string",
        },
      },
      telemetryType: {
        type: "string",
      },
      title: {
        type: "string",
      },
      url: {
        type: "string",
      },
      urlTimestampIndex: {
        type: "number",
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
      blockL10n: {
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
      content: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      isBlockable: {
        type: "boolean",
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
      buttons: {
        type: "array",
        items: {
          type: "object",
          required: ["l10n"],
          properties: {
            l10n: {
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
            url: {
              type: "string",
            },
          },
        },
      },
      // TODO: This is intended only for WebExtensions. We should remove it and
      // the WebExtensions urlbar API since we're no longer using it.
      buttonText: {
        type: "string",
      },
      // TODO: This is intended only for WebExtensions. We should remove it and
      // the WebExtensions urlbar API since we're no longer using it.
      buttonUrl: {
        type: "string",
      },
      // l10n { id, args }
      helpL10n: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "object",
            additionalProperties: true,
          },
        },
      },
      helpUrl: {
        type: "string",
      },
      icon: {
        type: "string",
      },
      // TODO: This is intended only for WebExtensions. We should remove it and
      // the WebExtensions urlbar API since we're no longer using it.
      text: {
        type: "string",
      },
      // l10n { id, args }
      titleL10n: {
        type: "object",
        required: ["id"],
        properties: {
          id: {
            type: "string",
          },
          args: {
            type: "object",
            additionalProperties: true,
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
          "dismissalAcknowledgment",
          "extension",
          "intervention_clear",
          "intervention_refresh",
          "intervention_update_ask",
          "intervention_update_refresh",
          "intervention_update_restart",
          "intervention_update_web",
          "searchTip_onboard",
          "searchTip_persist",
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
export class UrlbarQueryContext {
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
   * @param {Array} [options.sources]
   *   A list of acceptable UrlbarUtils.RESULT_SOURCE for the context.
   * @param {object} [options.searchMode]
   *   The input's current search mode.  See UrlbarInput.setSearchMode for a
   *   description.
   * @param {boolean} [options.prohibitRemoteResults]
   *   This provides a short-circuit override for `context.allowRemoteResults`.
   *   If it's false, then `allowRemoteResults` will do its usual checks to
   *   determine whether remote results are allowed. If it's true, then
   *   `allowRemoteResults` will immediately return false. Defaults to false.
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
      ["currentPage", v => typeof v == "string" && !!v.length],
      ["formHistoryName", v => typeof v == "string" && !!v.length],
      ["prohibitRemoteResults", v => true, false],
      ["providers", v => Array.isArray(v) && v.length],
      ["searchMode", v => v && typeof v == "object"],
      ["sources", v => Array.isArray(v) && v.length],
      ["view", v => true],
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
   * @param {Array} optionNames The names of the options to check for.
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
   *
   * @returns {{ href: string; isSearch: boolean; }?}
   */
  get fixupInfo() {
    if (!this._fixupError && !this._fixupInfo && this.trimmedSearchString) {
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
          scheme: info.fixedURI.scheme,
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
   *
   * @returns {any?}
   */
  get fixupError() {
    if (!this.fixupInfo) {
      return this._fixupError;
    }

    return null;
  }

  /**
   * Returns whether results from remote services are generally allowed for the
   * context. Callers can impose further restrictions as appropriate, but
   * typically they should not fetch remote results if this returns false.
   *
   * @param {string} [searchString]
   *   Usually this is just the context's search string, but if you need to
   *   fetch remote results based on a modified version, you can pass it here.
   * @param {boolean} [allowEmptySearchString]
   *   Whether to check for the minimum length of the search string.
   * @returns {boolean}
   *   Whether remote results are allowed.
   */
  allowRemoteResults(
    searchString = this.searchString,
    allowEmptySearchString = false
  ) {
    if (this.prohibitRemoteResults) {
      return false;
    }

    // We're unlikely to get useful remote results for a single character.
    if (searchString.length < 2 && !allowEmptySearchString) {
      return false;
    }

    // Disallow remote results if only an origin is typed to avoid disclosing
    // sites the user visits. This also catches partially typed origins, like
    // mozilla.o, because the fixup check below can't validate them.
    if (
      this.tokens.length == 1 &&
      this.tokens[0].type == lazy.UrlbarTokenizer.TYPE.POSSIBLE_ORIGIN
    ) {
      return false;
    }

    // Disallow remote results for strings containing tokens that look like URIs
    // to avoid disclosing information about networks and passwords.
    if (this.fixupInfo?.href && !this.fixupInfo?.isSearch) {
      return false;
    }

    // Allow remote results.
    return true;
  }
}

/**
 * Base class for a muxer.
 * The muxer scope is to sort a given list of results.
 */
export class UrlbarMuxer {
  /**
   * Unique name for the muxer, used by the context to sort results.
   * Not using a unique name will cause the newest registration to win.
   *
   * @abstract
   */
  get name() {
    return "UrlbarMuxerBase";
  }

  /**
   * Sorts queryContext results in-place.
   *
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
export class UrlbarProvider {
  constructor() {
    ChromeUtils.defineLazyGetter(this, "logger", () =>
      UrlbarUtils.getLogger({ prefix: `Provider.${this.name}` })
    );
  }

  /**
   * Unique name for the provider, used by the context to filter on providers.
   * Not using a unique name will cause the newest registration to win.
   *
   * @abstract
   */
  get name() {
    return "UrlbarProviderBase";
  }

  /**
   * The type of the provider, must be one of UrlbarUtils.PROVIDER_TYPE.
   *
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
      console.error(ex);
    }
    return undefined;
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
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
   *
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
   *
   * Note: Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @abstract
   */
  startQuery(queryContext, addCallback) {
    throw new Error("Trying to access the base class, must be overridden");
  }

  /**
   * Cancels a running query,
   *
   * @param {UrlbarQueryContext} queryContext the query context object to cancel
   *        query for.
   * @abstract
   */
  cancelQuery(queryContext) {
    // Override this with your clean-up on cancel code.
  }

  /**
   * Called when the user starts and ends an engagement with the urlbar.
   *
   * @param {string} state
   *   The state of the engagement, one of the following strings:
   *
   *   start
   *       A new query has started in the urlbar.
   *   engagement
   *       The user picked a result in the urlbar or used paste-and-go.
   *   abandonment
   *       The urlbar was blurred (i.e., lost focus).
   *   discard
   *       This doesn't correspond to a user action, but it means that the
   *       urlbar has discarded the engagement for some reason, and the
   *       `onEngagement` implementation should ignore it.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The engagement's query context.  This is *not* guaranteed to be defined
   *   when `state` is "start".  It will always be defined for "engagement" and
   *   "abandonment".
   * @param {object} details
   *   This object is non-empty only when `state` is "engagement" or
   *   "abandonment", and it describes the search string and engaged result.
   *
   *   For "engagement", it has the following properties:
   *
   *   {UrlbarResult} result
   *       The engaged result. If a result itself was picked, this will be it.
   *       If an element related to a result was picked (like a button or menu
   *       command), this will be that result. This property will be present if
   *       and only if `state` == "engagement", so it can be used to quickly
   *       tell when the user engaged with a result.
   *   {Element} element
   *       The picked DOM element.
   *   {boolean} isSessionOngoing
   *       True if the search session remains ongoing or false if the engagement
   *       ended it. Typically picking a result ends the session but not always.
   *       Picking a button or menu command may not end the session; dismissals
   *       do not, for example.
   *   {string} searchString
   *       The search string for the engagement's query.
   *   {number} selIndex
   *       The index of the picked result.
   *   {string} selType
   *       The type of the selected result.  See TelemetryEvent.record() in
   *       UrlbarController.jsm.
   *   {string} provider
   *       The name of the provider that produced the picked result.
   *
   *   For "abandonment", only `searchString` is defined.
   * @param {UrlbarController} controller
   *  The associated controller.
   */
  onEngagement(state, queryContext, details, controller) {}

  /**
   * Called before a result from the provider is selected. See `onSelection`
   * for details on what that means.
   *
   * @param {UrlbarResult} result
   *   The result that was selected.
   * @param {Element} element
   *   The element in the result's view that was selected.
   * @abstract
   */
  onBeforeSelection(result, element) {}

  /**
   * Called when a result from the provider is selected. "Selected" refers to
   * the user highlighing the result with the arrow keys/Tab, before it is
   * picked. onSelection is also called when a user clicks a result. In the
   * event of a click, onSelection is called just before onEngagement. Note that
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
   * Gets the list of commands that should be shown in the result menu for a
   * given result from the provider. All commands returned by this method should
   * be handled by implementing `onEngagement()` with the possible exception of
   * commands automatically handled by the urlbar, like "help".
   *
   * @param {UrlbarResult} result
   *   The menu will be shown for this result.
   * @returns {Array}
   *   If the result doesn't have any commands, this should return null.
   *   Otherwise it should return an array of command objects that look like:
   *   `{ name, l10n, children}`
   *
   *   {string} name
   *     The name of the command. Must be specified unless `children` is
   *     present. When a command is picked, its name will be passed as
   *     `details.selType` to `onEngagement()`. The special name "separator"
   *     will create a menu separator.
   *   {object} l10n
   *     An l10n object for the command's label: `{ id, args }`
   *     Must be specified unless `name` is "separator".
   *   {array} children
   *     If specified, a submenu will be created with the given child commands.
   *     Each object in the array must be a command object.
   */
  getResultCommands(result) {
    return null;
  }

  /**
   * Defines whether the view should defer user selection events while waiting
   * for the first result from this provider.
   *
   * Note: UrlbarEventBufferer has a timeout after which user events will be
   *       processed regardless.
   *
   * @returns {boolean} Whether the provider wants to defer user selection
   *          events.
   * @see {@link UrlbarEventBufferer}
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
export class SkippableTimer {
  /**
   * This can be used to track whether the timer completed.
   */
  done = false;

  /**
   * Creates a skippable timer for the given callback and time.
   *
   * @param {object} options An object that configures the timer
   * @param {string} options.name The name of the timer, logged when necessary
   * @param {Function} options.callback To be invoked when requested
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
          this.done = true;
          this._timer = null;
          resolve();
        },
        time,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
      this._log(`Started`);
    });

    let firePromise = new Promise(resolve => {
      this.fire = async () => {
        this.done = true;
        if (this._timer) {
          if (!this._canceled) {
            this._log(`Skipped`);
          }
          this._timer.cancel();
          this._timer = null;
          resolve();
        }
        await this.promise;
      };
    });

    this.promise = Promise.race([timerPromise, firePromise]).then(() => {
      // If we've been canceled, don't call back.
      if (callback && !this._canceled) {
        callback();
      }
    });
  }

  /**
   * Allows to cancel the timer and the callback won't be invoked.
   * It is not strictly necessary to await for this, the promise can just be
   * used to ensure all the internal work is complete.
   */
  async cancel() {
    if (this._timer) {
      this._log(`Canceling`);
      this._canceled = true;
    }
    await this.fire();
  }

  _log(msg, isError = false) {
    let line = `SkippableTimer :: ${this.name} :: ${msg}`;
    if (this.logger) {
      this.logger.debug(line);
    }
    if (isError) {
      console.error(line);
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
export class L10nCache {
  /**
   * @param {Localization} l10n
   *   A `Localization` object like `document.l10n`. This class keeps a weak
   *   reference to this object, so the caller or something else must hold onto
   *   it.
   */
  constructor(l10n) {
    this.l10n = Cu.getWeakReference(l10n);
    this.QueryInterface = ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]);
    Services.obs.addObserver(this, "intl:app-locales-changed", true);
  }

  /**
   * Gets a cached l10n message.
   *
   * @param {object} options
   *   Options
   * @param {string} options.id
   *   The string's Fluent ID.
   * @param {object} options.args
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   * @param {boolean} options.excludeArgsFromCacheKey
   *   Pass true if the string was cached using a key that excluded arguments.
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
  get({ id, args = undefined, excludeArgsFromCacheKey = false }) {
    return this._messagesByKey.get(
      this._key({ id, args, excludeArgsFromCacheKey })
    );
  }

  /**
   * Fetches a string from Fluent and caches it.
   *
   * @param {object} options
   *   Options
   * @param {string} options.id
   *   The string's Fluent ID.
   * @param {object} options.args
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   * @param {boolean} options.excludeArgsFromCacheKey
   *   Pass true to cache the string using a key that excludes the arguments.
   *   The string will be cached only by its ID. This is useful if the string is
   *   used only once in the UI, its arguments vary, and it's acceptable to
   *   fetch and show a cached value with old arguments until the string is
   *   relocalized with new arguments.
   */
  async add({ id, args = undefined, excludeArgsFromCacheKey = false }) {
    let l10n = this.l10n.get();
    if (!l10n) {
      return;
    }
    let messages = await l10n.formatMessages([{ id, args }]);
    if (!messages?.length) {
      console.error(
        "l10n.formatMessages returned an unexpected value for ID: ",
        id
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
    this._messagesByKey.set(
      this._key({ id, args, excludeArgsFromCacheKey }),
      message
    );
  }

  /**
   * Fetches and caches a string if it's not already cached. This is just a
   * slight optimization over `add` that avoids calling into Fluent
   * unnecessarily.
   *
   * @param {object} options
   *   Options
   * @param {string} options.id
   *   The string's Fluent ID.
   * @param {object} options.args
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   * @param {boolean} options.excludeArgsFromCacheKey
   *   Pass true to cache the string using a key that excludes the arguments.
   *   The string will be cached only by its ID. See `add()` for more.
   */
  async ensure({ id, args = undefined, excludeArgsFromCacheKey = false }) {
    // Always re-cache if `excludeArgsFromCacheKey` is true. The values in
    // `args` may be different from the values in the cached string.
    if (excludeArgsFromCacheKey || !this.get({ id, args })) {
      await this.add({ id, args, excludeArgsFromCacheKey });
    }
  }

  /**
   * Fetches and caches strings that aren't already cached.
   *
   * @param {Array} objects
   *   An array of objects as passed to `ensure()`.
   */
  async ensureAll(objects) {
    let promises = [];
    for (let obj of objects) {
      promises.push(this.ensure(obj));
    }
    await Promise.all(promises);
  }

  /**
   * Removes a cached string.
   *
   * @param {object} options
   *   Options
   * @param {string} options.id
   *   The string's Fluent ID.
   * @param {object} options.args
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   * @param {boolean} options.excludeArgsFromCacheKey
   *   Pass true if the string was cached using a key that excludes the
   *   arguments. If true, `args` is ignored.
   */
  delete({ id, args = undefined, excludeArgsFromCacheKey = false }) {
    this._messagesByKey.delete(
      this._key({ id, args, excludeArgsFromCacheKey })
    );
  }

  /**
   * Removes all cached strings.
   */
  clear() {
    this._messagesByKey.clear();
  }

  /**
   * Returns the number of cached messages.
   *
   * @returns {number}
   */
  size() {
    return this._messagesByKey.size;
  }

  /**
   * Observer method from Services.obs.addObserver.
   *
   * @param {nsISupports} subject
   *   The subject of the notification.
   * @param {string} topic
   *   The topic of the notification.
   * @param {string} data
   *   The data attached to the notification.
   */
  async observe(subject, topic, data) {
    switch (topic) {
      case "intl:app-locales-changed": {
        await this.l10n.ready;
        this.clear();
        break;
      }
    }
  }

  /**
   * Cache keys => cached message objects
   */
  _messagesByKey = new Map();

  /**
   * Returns a cache key for a string in `_messagesByKey`.
   *
   * @param {object} options
   *   Options
   * @param {string} options.id
   *   The string's Fluent ID.
   * @param {object} options.args
   *   The Fluent arguments as passed to `l10n.setAttributes`.
   * @param {boolean} options.excludeArgsFromCacheKey
   *   Pass true to exclude the arguments from the key and include only the ID.
   * @returns {string}
   *   The cache key.
   */
  _key({ id, args, excludeArgsFromCacheKey }) {
    // Keys are `id` plus JSON'ed `args` values. `JSON.stringify` doesn't
    // guarantee a particular ordering of object properties, so instead of
    // stringifying `args` as is, sort its entries by key and then pull out the
    // values. The final key is a JSON'ed array of `id` concatenated with the
    // sorted-by-key `args` values.
    args = (!excludeArgsFromCacheKey && args) || [];
    let argValues = Object.entries(args)
      .sort(([key1], [key2]) => key1.localeCompare(key2))
      .map(([_, value]) => value);
    let parts = [id].concat(argValues);
    return JSON.stringify(parts);
  }
}

/**
 * This class provides a way of serializing access to a resource. It's a queue
 * of callbacks (or "tasks") where each callback is called and awaited in order,
 * one at a time.
 */
export class TaskQueue {
  /**
   * @returns {Promise}
   *   Resolves when the queue becomes empty. If the queue is already empty,
   *   then a resolved promise is returned.
   */
  get emptyPromise() {
    if (!this._queue.length) {
      return Promise.resolve();
    }
    return new Promise(resolve => this._emptyCallbacks.push(resolve));
  }

  /**
   * Adds a callback function to the task queue. The callback will be called
   * after all other callbacks before it in the queue. This method returns a
   * promise that will be resolved after awaiting the callback. The promise will
   * be resolved with the value returned by the callback.
   *
   * @param {Function} callback
   *   The function to queue.
   * @returns {Promise}
   *   Resolved after the task queue calls and awaits `callback`. It will be
   *   resolved with the value returned by `callback`. If `callback` throws an
   *   error, then it will be rejected with the error.
   */
  queue(callback) {
    return new Promise((resolve, reject) => {
      this._queue.push({ callback, resolve, reject });
      if (this._queue.length == 1) {
        this._doNextTask();
      }
    });
  }

  /**
   * Calls the next function in the task queue and recurses until the queue is
   * empty. Once empty, all empty callback functions are called.
   */
  async _doNextTask() {
    if (!this._queue.length) {
      while (this._emptyCallbacks.length) {
        let callback = this._emptyCallbacks.shift();
        callback();
      }
      return;
    }

    let { callback, resolve, reject } = this._queue[0];
    try {
      let value = await callback();
      resolve(value);
    } catch (error) {
      console.error(error);
      reject(error);
    }
    this._queue.shift();
    this._doNextTask();
  }

  _queue = [];
  _emptyCallbacks = [];
}
