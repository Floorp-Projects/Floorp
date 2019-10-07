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

const RESULT_TYPE_TO_GROUP = new Map([
  [UrlbarUtils.RESULT_TYPE.TAB_SWITCH, UrlbarUtils.RESULT_GROUP.GENERAL],
  [UrlbarUtils.RESULT_TYPE.SEARCH, UrlbarUtils.RESULT_GROUP.SUGGESTION],
  [UrlbarUtils.RESULT_TYPE.URL, UrlbarUtils.RESULT_GROUP.GENERAL],
  [UrlbarUtils.RESULT_TYPE.KEYWORD, UrlbarUtils.RESULT_GROUP.GENERAL],
  [UrlbarUtils.RESULT_TYPE.OMNIBOX, UrlbarUtils.RESULT_GROUP.EXTENSION],
  [UrlbarUtils.RESULT_TYPE.REMOTE_TAB, UrlbarUtils.RESULT_GROUP.GENERAL],
  [UrlbarUtils.RESULT_TYPE.TIP, UrlbarUtils.RESULT_GROUP.GENERAL],
]);

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
    // other results, and all of them are searches.
    let searchInPrivateWindowIndex = context.results.findIndex(
      r => r.type == UrlbarUtils.RESULT_TYPE.SEARCH && r.payload.inPrivateWindow
    );
    if (
      searchInPrivateWindowIndex != -1 &&
      (context.results.length == 1 ||
        context.results.some(
          r =>
            r.type != UrlbarUtils.RESULT_TYPE.SEARCH || r.payload.keywordOffer
        ))
    ) {
      // Remove the result.
      context.results.splice(searchInPrivateWindowIndex, 1);
    }

    if (!context.results.length) {
      return;
    }
    // Look for an heuristic result.  If it's a preselected search result, use
    // search buckets, otherwise use normal buckets.
    let heuristicResult = context.results.find(r => r.heuristic);
    let buckets =
      context.preselected &&
      heuristicResult &&
      heuristicResult.type == UrlbarUtils.RESULT_TYPE.SEARCH
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
          result == heuristicResult &&
          context.preselected
        ) {
          // Handle the heuristic result.
          sortedResults.unshift(result);
          handled.add(result);
          context.maxResults -= UrlbarUtils.getSpanForResult(result) - 1;
          slots--;
        } else if (group == RESULT_TYPE_TO_GROUP.get(result.type)) {
          // If there's no suggestedIndex, insert the result now, otherwise
          // we'll handle it later.
          if (result.suggestedIndex < 0) {
            sortedResults.push(result);
          }
          handled.add(result);
          context.maxResults -= UrlbarUtils.getSpanForResult(result) - 1;
          slots--;
        } else if (!RESULT_TYPE_TO_GROUP.has(result.type)) {
          let errorMsg = `Result type ${
            result.type
          } is not mapped to a match group.`;
          logger.error(errorMsg);
          Cu.reportError(errorMsg);
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
