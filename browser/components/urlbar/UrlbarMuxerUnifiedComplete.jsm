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
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.repository.getLogger("Urlbar.Muxer.UnifiedComplete")
);

function groupFromResult(result) {
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
   * @param {UrlbarQueryContext} context The query context.
   */
  sort(context) {
    // Remove search suggestions that are duplicates of search history results.
    context.results = this._dedupeSearchHistoryAndSuggestions(context.results);

    // A Search in a Private Window result should only be shown when there are
    // other results, and all of them are searches. It should also not be shown
    // if the user typed an alias, because it's an explicit search engine choice.
    // We don't show it if there is a search history result. This is because
    // search history results are RESULT_TYPE.SEARCH but they arrive faster than
    // search suggestions in most cases because they are being fetched locally.
    // This leads to the private search result flickering as the suggestions
    // load in after the search history result.
    let searchInPrivateWindowIndex = context.results.findIndex(
      r => r.type == UrlbarUtils.RESULT_TYPE.SEARCH && r.payload.inPrivateWindow
    );
    if (
      searchInPrivateWindowIndex != -1 &&
      (context.results.length == 1 ||
        context.results.some(
          r =>
            r.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
            r.payload.keywordOffer ||
            (r.heuristic && r.payload.keyword)
        ))
    ) {
      // Remove the result.
      context.results.splice(searchInPrivateWindowIndex, 1);
    }
    if (!context.results.length) {
      return;
    }
    // Look for an heuristic result.  If it's a search result, use search
    // buckets, otherwise use normal buckets.
    let heuristicResult = context.results.find(r => r.heuristic);
    let buckets =
      heuristicResult && heuristicResult.type == UrlbarUtils.RESULT_TYPE.SEARCH
        ? UrlbarPrefs.get("matchBucketsSearch")
        : UrlbarPrefs.get("matchBuckets");
    logger.debug(`Buckets: ${buckets}`);
    // These results have a suggested index and should be moved if possible.
    // The sorting is important, to avoid messing up indices later when we'll
    // insert these results.
    let reshuffleResults = context.results
      .filter(r => r.suggestedIndex >= 0)
      .sort((a, b) => a.suggestedIndex - b.suggestedIndex);
    let sortedResults = [];
    // Track which results have been inserted already.
    let handled = new Set();
    for (let [group, slots] of buckets) {
      // Search all the available results to fill this bucket.
      for (let result of context.results) {
        if (slots == 0) {
          // There's no more space in this bucket.
          break;
        }
        if (handled.has(result)) {
          // Already handled.
          continue;
        }

        if (
          group == UrlbarUtils.RESULT_GROUP.HEURISTIC &&
          result == heuristicResult
        ) {
          // Handle the heuristic result.
          sortedResults.unshift(result);
          handled.add(result);
          slots--;
        } else if (group == groupFromResult(result)) {
          // If there's no suggestedIndex, insert the result now, otherwise
          // we'll handle it later.
          if (result.suggestedIndex < 0) {
            sortedResults.push(result);
          }
          handled.add(result);
          slots--;
        }
      }
    }
    for (let result of reshuffleResults) {
      if (sortedResults.length >= result.suggestedIndex) {
        sortedResults.splice(result.suggestedIndex, 0, result);
      } else {
        sortedResults.push(result);
      }
    }
    context.results = sortedResults;
  }

  /**
   * Takes a list of results and dedupes search history and suggestions. Prefers
   * search history. Also removes duplicate search history results.
   * @param {array} results
   *   A list of results from a UrlbarQueryContext.
   * @returns {array}
   *   The deduped list of results.
   */
  _dedupeSearchHistoryAndSuggestions(results) {
    if (
      !UrlbarPrefs.get("restyleSearches") ||
      !UrlbarPrefs.get("browser.search.suggest.enabled") ||
      !UrlbarPrefs.get("suggest.searches")
    ) {
      return results;
    }

    let suggestionResults = [];
    // historyEnginesBySuggestion maps:
    //   suggestion ->
    //     set of engines providing that suggestion from search history
    let historyEnginesBySuggestion = new Map();
    for (let i = 0; i < results.length; i++) {
      let result = results[i];
      if (
        !result.heuristic &&
        groupFromResult(result) == UrlbarUtils.RESULT_GROUP.SUGGESTION
      ) {
        if (result.payload.isSearchHistory) {
          let historyEngines = historyEnginesBySuggestion.get(
            result.payload.suggestion
          );
          if (!historyEngines) {
            historyEngines = new Set();
            historyEnginesBySuggestion.set(
              result.payload.suggestion,
              historyEngines
            );
          }
          historyEngines.add(result.payload.engine);
        } else {
          // Unshift so that we iterate and remove in reverse order below.
          suggestionResults.unshift([result, i]);
        }
      }
    }
    for (
      let i = 0;
      historyEnginesBySuggestion.size && i < suggestionResults.length;
      i++
    ) {
      let [result, index] = suggestionResults[i];
      let historyEngines = historyEnginesBySuggestion.get(
        result.payload.suggestion
      );
      if (historyEngines && historyEngines.has(result.payload.engine)) {
        // This suggestion result has the same suggestion and engine as a search
        // history result.
        results.splice(index, 1);
        historyEngines.delete(result.payload.engine);
        if (!historyEngines.size) {
          historyEnginesBySuggestion.delete(result.payload.suggestion);
        }
      }
    }

    return results;
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
