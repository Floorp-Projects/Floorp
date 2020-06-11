/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a component used to sort results in a UrlbarQueryContext.
 */

var EXPORTED_SYMBOLS = ["UrlbarMuxerUnifiedComplete"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesSearchAutocompleteProvider:
    "resource://gre/modules/PlacesSearchAutocompleteProvider.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Muxer.UnifiedComplete")
);

function groupFromResult(result) {
  if (result.heuristic) {
    return UrlbarUtils.RESULT_GROUP.HEURISTIC;
  }
  switch (result.type) {
    case UrlbarUtils.RESULT_TYPE.SEARCH:
      return result.payload.suggestion
        ? UrlbarUtils.RESULT_GROUP.SUGGESTION
        : UrlbarUtils.RESULT_GROUP.GENERAL;
    case UrlbarUtils.RESULT_TYPE.OMNIBOX:
      return UrlbarUtils.RESULT_GROUP.EXTENSION;
    default:
      return UrlbarUtils.RESULT_GROUP.GENERAL;
  }
}

/**
 * Class used to create a muxer.
 * The muxer receives and sorts results in a UrlbarQueryContext.
 */
class MuxerUnifiedComplete extends UrlbarMuxer {
  constructor() {
    super();
  }

  get name() {
    return "UnifiedComplete";
  }

  /**
   * Sorts results in the given UrlbarQueryContext.
   *
   * @param {UrlbarQueryContext} context
   *   The query context.
   */
  sort(context) {
    // This method is called multiple times per keystroke, so it should be as
    // fast and efficient as possible.  We do two passes through the results:
    // one to collect info for the second pass, and then a second to build the
    // unsorted list of results.  If you find yourself writing something like
    // context.results.find(), filter(), sort(), etc., modify one or both passes
    // instead.

    // Capture information about the heuristic result to dedupe results from the
    // heuristic more quickly.
    let heuristicResultQuery;
    let heuristicResultOmniboxContent;
    if (context.heuristicResult) {
      if (
        context.heuristicResult.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        context.heuristicResult.payload.query
      ) {
        heuristicResultQuery = context.heuristicResult.payload.query.toLocaleLowerCase();
      } else if (
        context.heuristicResult.type == UrlbarUtils.RESULT_TYPE.OMNIBOX &&
        context.heuristicResult.payload.content
      ) {
        heuristicResultOmniboxContent = context.heuristicResult.payload.content.toLocaleLowerCase();
      }
    }

    let canShowPrivateSearch = context.results.length > 1;
    let canShowTailSuggestions = true;
    let resultsWithSuggestedIndex = [];
    let formHistoryResults = new Set();
    let formHistorySuggestions = new Set();
    let maxFormHistoryCount = Math.min(
      UrlbarPrefs.get("maxHistoricalSearchSuggestions"),
      context.maxResults
    );

    // Do the first pass through the results.  We only collect info for the
    // second pass here.
    for (let result of context.results) {
      // The "Search in a Private Window" result should only be shown when there
      // are other results and all of them are searches.  It should not be shown
      // if the user typed an alias because that's an explicit engine choice.
      if (
        canShowPrivateSearch &&
        (result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
          result.payload.keywordOffer ||
          (result.heuristic && result.payload.keyword))
      ) {
        canShowPrivateSearch = false;
      }

      // Include form history up to the max count that doesn't dupe the
      // heuristic.  The search suggestions provider fetches max count + 1 form
      // history results so that the muxer can exclude a result that equals the
      // heuristic if necessary.
      if (
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.source == UrlbarUtils.RESULT_SOURCE.HISTORY &&
        formHistoryResults.size < maxFormHistoryCount &&
        result.payload.lowerCaseSuggestion &&
        result.payload.lowerCaseSuggestion != heuristicResultQuery
      ) {
        formHistoryResults.add(result);
        formHistorySuggestions.add(result.payload.lowerCaseSuggestion);
      }

      // If we find results other than the heuristic, "Search in Private
      // Window," or tail suggestions on the first pass, we should hide tail
      // suggestions on the second, since tail suggestions are a "last resort".
      if (
        canShowTailSuggestions &&
        !result.heuristic &&
        (result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
          (!result.payload.inPrivateWindow && !result.payload.tail))
      ) {
        canShowTailSuggestions = false;
      }
    }

    // Do the second pass through results to build the list of unsorted results.
    let unsortedResults = [];
    for (let result of context.results) {
      // Exclude "Search in a Private Window" as determined in the first pass.
      if (
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.payload.inPrivateWindow &&
        !canShowPrivateSearch
      ) {
        continue;
      }

      // Save suggestedIndex results for later.
      if (result.suggestedIndex >= 0) {
        resultsWithSuggestedIndex.push(result);
        continue;
      }

      // Exclude form history as determined in the first pass.
      if (
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.source == UrlbarUtils.RESULT_SOURCE.HISTORY &&
        !formHistoryResults.has(result)
      ) {
        continue;
      }

      // Exclude remote search suggestions that dupe the heuristic.  We also
      // want to exclude remote suggestions that dupe form history, but that's
      // already been done by the search suggestions controller.
      if (
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.source == UrlbarUtils.RESULT_SOURCE.SEARCH &&
        result.payload.lowerCaseSuggestion &&
        result.payload.lowerCaseSuggestion === heuristicResultQuery
      ) {
        continue;
      }

      // Exclude tail suggestions if we have non-tail suggestions.
      if (
        !canShowTailSuggestions &&
        groupFromResult(result) == UrlbarUtils.RESULT_GROUP.SUGGESTION &&
        result.payload.tail
      ) {
        continue;
      }

      // Exclude SERPs from browser history that dupe either the heuristic or
      // included form history.
      if (
        result.source == UrlbarUtils.RESULT_SOURCE.HISTORY &&
        result.type == UrlbarUtils.RESULT_TYPE.URL
      ) {
        let submission;
        try {
          // parseSubmissionURL throws if PlacesSearchAutocompleteProvider
          // hasn't finished initializing, so try-catch this call.  There's no
          // harm if it throws, we just won't dedupe SERPs this time.
          submission = PlacesSearchAutocompleteProvider.parseSubmissionURL(
            result.payload.url
          );
        } catch (error) {}
        if (submission) {
          let resultQuery = submission.terms.toLocaleLowerCase();
          if (
            heuristicResultQuery === resultQuery ||
            formHistorySuggestions.has(resultQuery)
          ) {
            // If the result's URL is the same as a brand new SERP URL created
            // from the query string modulo certain URL params, then treat the
            // result as a dupe and exclude it.
            let [newSerpURL] = UrlbarUtils.getSearchQueryUrl(
              submission.engine,
              resultQuery
            );
            if (this._serpURLsHaveSameParams(newSerpURL, result.payload.url)) {
              continue;
            }
          }
        }
      }

      // Exclude omnibox results that dupe the heuristic.
      if (
        !result.heuristic &&
        result.type == UrlbarUtils.RESULT_TYPE.OMNIBOX &&
        result.payload.content == heuristicResultOmniboxContent
      ) {
        continue;
      }

      // Include this result.
      unsortedResults.push(result);
    }

    // If the heuristic result is a search result, use search buckets, otherwise
    // use normal buckets.
    let buckets =
      context.heuristicResult &&
      context.heuristicResult.type == UrlbarUtils.RESULT_TYPE.SEARCH
        ? UrlbarPrefs.get("matchBucketsSearch")
        : UrlbarPrefs.get("matchBuckets");
    logger.debug(`Buckets: ${buckets}`);

    // Finally, build the sorted list of results.  Fill each bucket in turn.
    let sortedResults = [];
    let handledResults = new Set();
    let count = Math.min(unsortedResults.length, context.maxResults);
    for (let b = 0; handledResults.size < count && b < buckets.length; b++) {
      let [group, slotCount] = buckets[b];
      // Search all the available results to fill this bucket.
      for (
        let i = 0;
        slotCount && handledResults.size < count && i < unsortedResults.length;
        i++
      ) {
        let result = unsortedResults[i];
        if (!handledResults.has(result) && group == groupFromResult(result)) {
          sortedResults.push(result);
          handledResults.add(result);
          slotCount--;
        }
      }
    }

    // These results have a suggested index and should be moved if possible.
    // The sorting is important, to avoid messing up indices later when we'll
    // insert these results.
    resultsWithSuggestedIndex.sort(
      (a, b) => a.suggestedIndex - b.suggestedIndex
    );
    for (let result of resultsWithSuggestedIndex) {
      let index =
        result.suggestedIndex <= sortedResults.length
          ? result.suggestedIndex
          : sortedResults.length;
      sortedResults.splice(index, 0, result);
    }

    context.results = sortedResults;
  }

  /**
   * This is a helper for determining whether two SERP URLs are the same for the
   * purpose of deduping them.  This method checks only URL params, not domains.
   *
   * @param {string} url1
   *   The first URL.
   * @param {string} url2
   *   The second URL.
   * @returns {boolean}
   *   True if the two URLs have the same URL params for the purpose of deduping
   *   them.
   */
  _serpURLsHaveSameParams(url1, url2) {
    let params1 = new URL(url1).searchParams;
    let params2 = new URL(url2).searchParams;
    // Currently we are conservative, and the two URLs must have exactly the
    // same params except for "client" for us to consider them the same.
    for (let params of [params1, params2]) {
      params.delete("client");
    }
    // Check that each remaining url1 param is in url2, and vice versa.
    for (let [p1, p2] of [
      [params1, params2],
      [params2, params1],
    ]) {
      for (let [key, value] of p1) {
        if (!p2.getAll(key).includes(value)) {
          return false;
        }
      }
    }
    return true;
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
