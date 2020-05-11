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
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
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
   * @returns {boolean}
   *   True if the muxer sorted the results and false if not.  The muxer may
   *   decide it can't sort if there aren't yet enough results to make good
   *   decisions, for example to avoid flicker in the view.
   */
  sort(context) {
    // This method is called multiple times per keystroke, so it should be as
    // fast and efficient as possible.  We do one pass through active providers
    // and two passes through the results: one to collect info for the second
    // pass, and then a second to build the unsorted list of results.  If you
    // find yourself writing something like context.results.find(), filter(),
    // sort(), etc., modify one or both passes instead.

    // Collect info from the active providers.
    for (let providerName of context.activeProviders) {
      let provider = UrlbarProvidersManager.getProvider(providerName);

      // If the provider of the heuristic result is still active and the result
      // hasn't been created yet, bail.  Otherwise we may show another result
      // first and then later replace it with the heuristic, causing flicker.
      if (
        provider.type == UrlbarUtils.PROVIDER_TYPE.HEURISTIC &&
        !context.heuristicResult
      ) {
        return false;
      }
    }

    let heuristicResultQuery;
    if (
      context.heuristicResult &&
      context.heuristicResult.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      context.heuristicResult.payload.query
    ) {
      heuristicResultQuery = context.heuristicResult.payload.query.toLocaleLowerCase();
    }

    // We update canShowPrivateSearch below too.  This is its initial value.
    let canShowPrivateSearch = context.results.length > 1;
    let resultsWithSuggestedIndex = [];

    // Do the first pass through the results.  We only collect info for the
    // second pass here.
    for (let result of context.results) {
      // The "Search in a Private Window" result should only be shown when there
      // are other results and all of them are searches.  It should not be shown
      // if the user typed an alias because that's an explicit engine choice.
      if (
        result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        result.payload.keywordOffer ||
        (result.heuristic && result.payload.keyword)
      ) {
        canShowPrivateSearch = false;
        break;
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

      // Exclude remote search suggestions that dupe the heuristic result.
      if (
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.payload.suggestion &&
        result.payload.suggestion.toLocaleLowerCase() === heuristicResultQuery
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
    return true;
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
