/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that provides a heuristic result. The result
 * either vists a URL or does a search with the current engine. This result is
 * always the ultimate fallback for any query, so this provider is always active.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderHeuristicFallback"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Class used to create the provider.
 */
class ProviderHeuristicFallback extends UrlbarProvider {
  constructor() {
    super();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "HeuristicFallback";
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
  isActive(queryContext) {
    return true;
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
    let instance = this.queryInstance;

    let result = this._matchUnknownUrl(queryContext);
    if (result) {
      addCallback(this, result);
      // Since we can't tell if this is a real URL and whether the user wants
      // to visit or search for it, we provide an alternative searchengine
      // match if the string looks like an alphanumeric origin or an e-mail.
      let str = queryContext.searchString;
      try {
        new URL(str);
      } catch (ex) {
        if (
          UrlbarPrefs.get("keyword.enabled") &&
          (UrlbarTokenizer.looksLikeOrigin(str, {
            noIp: true,
            noPort: true,
          }) ||
            UrlbarTokenizer.REGEXP_COMMON_EMAIL.test(str))
        ) {
          let searchResult = this._engineSearchResult(queryContext);
          if (instance != this.queryInstance) {
            return;
          }
          addCallback(this, searchResult);
        }
      }
      return;
    }

    result = this._searchModeKeywordResult(queryContext);
    if (result) {
      addCallback(this, result);
      return;
    }

    result = this._engineSearchResult(queryContext);
    if (instance != this.queryInstance) {
      return;
    }
    if (result) {
      result.heuristic = true;
      addCallback(this, result);
    }
  }

  // TODO (bug 1054814): Use visited URLs to inform which scheme to use, if the
  // scheme isn't specificed.
  _matchUnknownUrl(queryContext) {
    // The user may have typed something like "word?" to run a search.  We
    // should not convert that to a URL.  We should also never convert actual
    // URLs into URL results when search mode is active or a search mode
    // restriction token was typed.
    if (
      queryContext.restrictSource == UrlbarUtils.RESULT_SOURCE.SEARCH ||
      UrlbarTokenizer.SEARCH_MODE_RESTRICT.has(
        queryContext.restrictToken?.value
      ) ||
      queryContext.searchMode
    ) {
      return null;
    }

    let unescapedSearchString = UrlbarUtils.unEscapeURIForUI(
      queryContext.searchString
    );
    let [prefix, suffix] = UrlbarUtils.stripURLPrefix(unescapedSearchString);
    if (!suffix && prefix) {
      // The user just typed a stripped protocol, don't build a non-sense url
      // like http://http/ for it.
      return null;
    }

    let searchUrl = queryContext.trimmedSearchString;

    if (queryContext.fixupError) {
      if (
        queryContext.fixupError == Cr.NS_ERROR_MALFORMED_URI &&
        !UrlbarPrefs.get("keyword.enabled")
      ) {
        let result = new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
          ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
            title: [searchUrl, UrlbarUtils.HIGHLIGHT.NONE],
            url: [searchUrl, UrlbarUtils.HIGHLIGHT.NONE],
          })
        );
        result.heuristic = true;
        return result;
      }

      return null;
    }

    // If the URI cannot be fixed or the preferred URI would do a keyword search,
    // that basically means this isn't useful to us. Note that
    // fixupInfo.keywordAsSent will never be true if the keyword.enabled pref
    // is false or there are no engines, so in that case we will always return
    // a "visit".
    if (!queryContext.fixupInfo?.href || queryContext.fixupInfo?.isSearch) {
      return null;
    }

    let uri = new URL(queryContext.fixupInfo.href);
    // Check the host, as "http:///" is a valid nsIURI, but not useful to us.
    // But, some schemes are expected to have no host. So we check just against
    // schemes we know should have a host. This allows new schemes to be
    // implemented without us accidentally blocking access to them.
    let hostExpected = ["http:", "https:", "ftp:", "chrome:"].includes(
      uri.protocol
    );
    if (hostExpected && !uri.host) {
      return null;
    }

    // getFixupURIInfo() escaped the URI, so it may not be pretty.  Embed the
    // escaped URL in the result since that URL should be "canonical".  But
    // pass the pretty, unescaped URL as the result's title, since it is
    // displayed to the user.
    let escapedURL = uri.toString();
    let displayURL = decodeURI(uri);

    // We don't know if this url is in Places or not, and checking that would
    // be expensive. Thus we also don't know if we may have an icon.
    // If we'd just try to fetch the icon for the typed string, we'd cause icon
    // flicker, since the url keeps changing while the user types.
    // By default we won't provide an icon, but for the subset of urls with a
    // host we'll check for a typed slash and set favicon for the host part.
    let iconUri;
    if (hostExpected && (searchUrl.endsWith("/") || uri.pathname.length > 1)) {
      // Look for an icon with the entire URL except for the pathname, including
      // scheme, usernames, passwords, hostname, and port.
      let pathIndex = uri.toString().lastIndexOf(uri.pathname);
      let prePath = uri.toString().slice(0, pathIndex);
      iconUri = `page-icon:${prePath}/`;
    }

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        title: [displayURL, UrlbarUtils.HIGHLIGHT.NONE],
        url: [escapedURL, UrlbarUtils.HIGHLIGHT.NONE],
        icon: iconUri,
      })
    );
    result.heuristic = true;
    return result;
  }

  _searchModeKeywordResult(queryContext) {
    if (!queryContext.tokens.length) {
      return null;
    }

    let firstToken = queryContext.tokens[0].value;
    if (!UrlbarTokenizer.SEARCH_MODE_RESTRICT.has(firstToken)) {
      return null;
    }

    // At this point, the search string starts with a token that can be
    // converted into search mode.
    // Now we need to determine what to do based on the remainder of the search
    // string.  If the remainder starts with a space, then we should enter
    // search mode, so we should continue below and create the result.
    // Otherwise, we should not enter search mode, and in that case, the search
    // string will look like one of the following:
    //
    // * The search string ends with the restriction token (e.g., the user
    //   has typed only the token by itself, with no trailing spaces).
    // * More tokens exist, but there's no space between the restriction
    //   token and the following token.  This is possible because the tokenizer
    //   does not require spaces between a restriction token and the remainder
    //   of the search string.  In this case, we should not enter search mode.
    //
    // If we return null here and thereby do not enter search mode, then we'll
    // continue on to _engineSearchResult, and the heuristic will be a
    // default engine search result.
    let query = UrlbarUtils.substringAfter(
      queryContext.searchString,
      firstToken
    );
    if (!UrlbarTokenizer.REGEXP_SPACES_START.test(query)) {
      return null;
    }

    let result;
    if (queryContext.restrictSource == UrlbarUtils.RESULT_SOURCE.SEARCH) {
      result = this._engineSearchResult(queryContext, firstToken);
    } else {
      result = new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          query: [query.trimStart(), UrlbarUtils.HIGHLIGHT.NONE],
          keyword: [firstToken, UrlbarUtils.HIGHLIGHT.NONE],
        })
      );
    }
    result.heuristic = true;
    return result;
  }

  _engineSearchResult(queryContext, keyword = null) {
    let engine;
    if (queryContext.searchMode?.engineName) {
      engine = Services.search.getEngineByName(
        queryContext.searchMode.engineName
      );
    } else {
      engine = UrlbarSearchUtils.getDefaultEngine(queryContext.isPrivate);
    }

    if (!engine) {
      return null;
    }

    // Strip a leading search restriction char, because we prepend it to text
    // when the search shortcut is used and it's not user typed. Don't strip
    // other restriction chars, so that it's possible to search for things
    // including one of those (e.g. "c#").
    let query = queryContext.searchString;
    if (
      queryContext.tokens[0] &&
      queryContext.tokens[0].value === UrlbarTokenizer.RESTRICT.SEARCH
    ) {
      query = UrlbarUtils.substringAfter(
        query,
        queryContext.tokens[0].value
      ).trim();
    }

    return new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
        engine: [engine.name, UrlbarUtils.HIGHLIGHT.TYPED],
        icon: engine.iconURI?.spec,
        query: [query, UrlbarUtils.HIGHLIGHT.NONE],
        keyword: keyword ? [keyword, UrlbarUtils.HIGHLIGHT.NONE] : undefined,
      })
    );
  }
}

var UrlbarProviderHeuristicFallback = new ProviderHeuristicFallback();
