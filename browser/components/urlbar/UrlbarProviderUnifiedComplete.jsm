/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a provider that wraps the existing UnifiedComplete
 * component, it is supposed to be used as an interim solution while we rewrite
 * the model providers in a more modular way.
 */

var EXPORTED_SYMBOLS = ["UrlbarProviderUnifiedComplete"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  UrlbarMatch: "resource:///modules/UrlbarMatch.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "unifiedComplete",
  "@mozilla.org/autocomplete/search;1?name=unifiedcomplete",
  "nsIAutoCompleteSearch");

XPCOMUtils.defineLazyGetter(this, "logger",
  () => Log.repository.getLogger("Places.Urlbar.Provider.UnifiedComplete"));

// See UnifiedComplete.
const TITLE_TAGS_SEPARATOR = " \u2013 ";

/**
 * Class used to create the provider.
 */
class ProviderUnifiedComplete {
  constructor() {
    // Maps the running queries by queryContext.
    this.queries = new Map();
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "UnifiedComplete";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.IMMEDIATE;
  }

  /**
   * Returns the sources returned by this provider.
   * @returns {array} one or multiple types from UrlbarUtils.MATCH_SOURCE.*
   */
  get sources() {
    return [
      UrlbarUtils.MATCH_SOURCE.TABS,
    ];
  }

  /**
   * Starts querying.
   * @param {object} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        match.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    logger.info(`Starting query for ${queryContext.searchString}`);
    let instance = {};
    this.queries.set(queryContext, instance);

    // Supported search params are:
    //  * "enable-actions": default to true.
    //  * "disable-private-actions": set for private windows, if not in permanent
    //    private browsing mode. ()
    //  * "private-window": the search is taking place in a private window.
    //  * "user-context-id:#": the userContextId to use.
    let params = ["enable-actions"];
    params.push(`max-results:${queryContext.maxResults}`);
    if (queryContext.isPrivate) {
      params.push("private-window");
      if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
        params.push("disable-private-actions");
      }
    }
    if (queryContext.userContextId) {
      params.push(`user-context-id:${queryContext.userContextId}}`);
    }

    let urls = new Set();
    await new Promise(resolve => {
      let listener = {
        onSearchResult(_, result) {
          let {done, matches} = convertResultToMatches(queryContext, result, urls);
          for (let match of matches) {
            addCallback(this, match);
          }
          if (done) {
            resolve();
          }
        },
      };
      unifiedComplete.startSearch(queryContext.searchString,
                                  params.join(" "),
                                  null, // previousResult
                                  listener);
    });

    // We are done.
    this.queries.delete(queryContext);
  }

  /**
   * Cancels a running query.
   * @param {object} queryContext The query context object
   */
  cancelQuery(queryContext) {
    logger.info(`Canceling query for ${queryContext.searchString}`);
    // This doesn't properly support being used concurrently by multiple fields.
    this.queries.delete(queryContext);
    unifiedComplete.stopSearch();
  }
}

var UrlbarProviderUnifiedComplete = new ProviderUnifiedComplete();

/**
 * Convert from a nsIAutocompleteResult to a list of new matches.
 * Note that at every call we get the full set of matches, included the
 * previously returned ones, and new matches may be inserted in the middle.
 * This means we could sort these wrongly, the muxer should take care of it.
 * In any case at least we're sure there's just one heuristic result and it
 * comes first.
 *
 * @param {object} context the QueryContext
 * @param {object} result an nsIAutocompleteResult
 * @param {set} urls a Set containing all the found urls, used to discard
 *        already added matches.
 * @returns {object} { matches: {array}, done: {boolean} }
 */
function convertResultToMatches(context, result, urls) {
  let matches = [];
  let done = [
    Ci.nsIAutoCompleteResult.RESULT_IGNORED,
    Ci.nsIAutoCompleteResult.RESULT_FAILURE,
    Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
    Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
  ].includes(result.searchResult) || result.errorDescription;

  for (let i = 0; i < result.matchCount; ++i) {
    // First, let's check if we already added this match.
    // nsIAutocompleteResult always contains all of the matches, includes ones
    // we may have added already. This means we'll end up adding things in the
    // wrong order here, but that's a task for the UrlbarMuxer.
    let url = result.getFinalCompleteValueAt(i);
    if (urls.has(url)) {
      continue;
    }
    urls.add(url);
    // Not used yet: result.getValueAt(i), result.getLabelAt(i)
    let style = result.getStyleAt(i);
    let match = makeUrlbarMatch(context.tokens, {
      url,
      icon: result.getImageAt(i),
      style,
      comment: result.getCommentAt(i),
      firstToken: context.tokens[0],
    });
    // Should not happen, but better safe than sorry.
    if (!match) {
      continue;
    }
    matches.push(match);
    // Manage autofill and preselected properties for the first match.
    if (i == 0 && result.defaultIndex == 0) {
      if (style.includes("autofill")) {
        context.autofill = true;
      }
      if (style.includes("heuristic")) {
        context.preselected = true;
      }
    }
  }
  return {matches, done};
}

/**
 * Creates a new UrlbarMatch from the provided data.
 * @param {array} tokens the search tokens.
 * @param {object} info includes properties from the legacy match.
 * @returns {object} an UrlbarMatch
 */
function makeUrlbarMatch(tokens, info) {
  let action = PlacesUtils.parseActionUrl(info.url);
  if (action) {
    switch (action.type) {
      case "searchengine":
        return new UrlbarMatch(
          UrlbarUtils.MATCH_TYPE.SEARCH,
          UrlbarUtils.MATCH_SOURCE.SEARCH,
          ...UrlbarMatch.payloadAndSimpleHighlights(tokens, {
            engine: [action.params.engineName, true],
            suggestion: [action.params.searchSuggestion, true],
            keyword: [action.params.alias, true],
            query: [action.params.searchQuery, true],
            icon: [info.icon, false],
          })
        );
      case "keyword":
        return new UrlbarMatch(
          UrlbarUtils.MATCH_TYPE.KEYWORD,
          UrlbarUtils.MATCH_SOURCE.BOOKMARKS,
          ...UrlbarMatch.payloadAndSimpleHighlights(tokens, {
            url: [action.params.url, true],
            keyword: [info.firstToken, true],
            postData: [action.params.postData, false],
            icon: [info.icon, false],
          })
        );
      case "extension":
        return new UrlbarMatch(
          UrlbarUtils.MATCH_TYPE.OMNIBOX,
          UrlbarUtils.MATCH_SOURCE.OTHER_NETWORK,
          ...UrlbarMatch.payloadAndSimpleHighlights(tokens, {
            title: [info.comment, true],
            content: [action.params.content, true],
            keyword: [action.params.keyword, true],
            icon: [info.icon, false],
          })
        );
      case "remotetab":
        return new UrlbarMatch(
          UrlbarUtils.MATCH_TYPE.REMOTE_TAB,
          UrlbarUtils.MATCH_SOURCE.TABS,
          ...UrlbarMatch.payloadAndSimpleHighlights(tokens, {
            url: [action.params.url, true],
            title: [info.comment, true],
            device: [action.params.deviceName, true],
            icon: [info.icon, false],
          })
        );
      case "visiturl":
        return new UrlbarMatch(
          UrlbarUtils.MATCH_TYPE.URL,
          UrlbarUtils.MATCH_SOURCE.OTHER_LOCAL,
          ...UrlbarMatch.payloadAndSimpleHighlights(tokens, {
            title: [info.comment, true],
            url: [action.params.url, true],
            icon: [info.icon, false],
          })
        );
      default:
        Cu.reportError("Unexpected action type");
        return null;
    }
  }

  if (info.style.includes("priority-search")) {
    return new UrlbarMatch(
      UrlbarUtils.MATCH_TYPE.SEARCH,
      UrlbarUtils.MATCH_SOURCE.SEARCH,
      ...UrlbarMatch.payloadAndSimpleHighlights(tokens, {
        engine: [info.comment, true],
        icon: [info.icon, false],
      })
    );
  }

  // This is a normal url/title tuple.
  let source, tags, comment;
  let hasTags = info.style.includes("tag");
  if (info.style.includes("bookmark") || hasTags) {
    source = UrlbarUtils.MATCH_SOURCE.BOOKMARKS;
    if (info.style.includes("tag")) {
      // Split title and tags.
      [comment, tags] = info.comment.split(TITLE_TAGS_SEPARATOR);
      tags = tags.split(",").map(t => t.trim());
    }
  } else if (info.style.includes("preloaded-top-sites")) {
    source = UrlbarUtils.MATCH_SOURCE.OTHER_LOCAL;
  } else {
    source = UrlbarUtils.MATCH_SOURCE.HISTORY;
  }
  return new UrlbarMatch(
    UrlbarUtils.MATCH_TYPE.URL,
    source,
    ...UrlbarMatch.payloadAndSimpleHighlights(tokens, {
      url: [info.url, true],
      icon: [info.icon, false],
      title: [comment, true],
      tags: [tags, true],
    })
  );
}
