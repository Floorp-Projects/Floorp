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
    // A Search in a Private Window result should only be shown when there are
    // other results, and all of them are searches. It should also not be shown
    // if the user typed an alias, because it's an explicit search engine choice.
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
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
