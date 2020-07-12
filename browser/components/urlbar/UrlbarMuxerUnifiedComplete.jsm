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
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  UrlbarUtils.getLogger({ prefix: "MuxerUnifiedComplete" })
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

// Breaks ties among heuristic results. Providers higher up the list are higher
// priority.
const heuristicOrder = [
  // Test providers are handled in sort(),
  // Extension providers are handled in sort(),
  "UrlbarProviderSearchTips",
  "Omnibox",
  "UnifiedComplete",
  "Autofill",
  "TokenAliasEngines",
  "HeuristicFallback",
];
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

  /* eslint-disable complexity */
  /**
   * Sorts results in the given UrlbarQueryContext.
   *
   * sort() is not suitable to be broken up into smaller functions or to rely
   * on more convenience functions. It exists to efficiently group many
   * conditions into just three loops. As a result, we must disable complexity
   * linting.
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
    let topHeuristicRank = Infinity;
    for (let result of context.allHeuristicResults) {
      // Determine the highest-ranking heuristic result.
      if (!result.heuristic) {
        continue;
      }

      // + 2 to reserve the highest-priority slots for test and extension
      // providers.
      let heuristicRank = heuristicOrder.indexOf(result.providerName) + 2;
      // Extension and test provider names vary widely and aren't suitable
      // for a static safelist like heuristicOrder.
      if (result.providerType == UrlbarUtils.PROVIDER_TYPE.EXTENSION) {
        heuristicRank = 1;
      } else if (result.providerName.startsWith("TestProvider")) {
        heuristicRank = 0;
      } else if (heuristicRank - 2 == -1) {
        throw new Error(
          `Heuristic result returned by unexpected provider: ${result.providerName}`
        );
      }
      // Replace in case of ties, which would occur if a provider sent two
      // heuristic results.
      if (heuristicRank <= topHeuristicRank) {
        topHeuristicRank = heuristicRank;
        context.heuristicResult = result;
      }
    }

    let heuristicResultQuery;
    if (context.heuristicResult) {
      if (
        context.heuristicResult.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        context.heuristicResult.payload.query
      ) {
        heuristicResultQuery = context.heuristicResult.payload.query.toLocaleLowerCase();
      }
    }

    let canShowPrivateSearch = context.results.length > 1;
    let canShowTailSuggestions = true;
    let resultsWithSuggestedIndex = [];
    let formHistoryResults = new Set();
    let formHistorySuggestions = new Set();
    // Used to deduped URLs based on their stripped prefix and title. Schema:
    //   {string} strippedUrl => {prefix, title, rank}
    let strippedUrlToTopPrefixAndTitle = new Map();
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

      if (result.type == UrlbarUtils.RESULT_TYPE.URL && result.payload.url) {
        let [strippedUrl, prefix] = UrlbarUtils.stripPrefixAndTrim(
          result.payload.url,
          {
            stripHttp: true,
            stripHttps: true,
            stripWww: true,
            trimEmptyQuery: true,
          }
        );
        let prefixRank = UrlbarUtils.getPrefixRank(prefix);
        let topPrefixData = strippedUrlToTopPrefixAndTitle.get(strippedUrl);
        let topPrefixRank = topPrefixData ? topPrefixData.rank : -1;
        if (topPrefixRank < prefixRank) {
          strippedUrlToTopPrefixAndTitle.set(strippedUrl, {
            prefix,
            title: result.payload.title,
            rank: prefixRank,
          });
        }
      }
    }

    // Do the second pass through results to build the list of unsorted results.
    let unsortedResults = [];
    for (let result of context.results) {
      // Exclude low-ranked heuristic results.
      if (result.heuristic && result != context.heuristicResult) {
        continue;
      }

      // We expect UnifiedComplete sent us the highest-ranked www. and non-www
      // origins, if any. Now, compare them to each other and to the heuristic
      // result.
      // 1. If the heuristic result is lower ranked than both, discard the www
      //    origin, unless it has a different page title than the non-www
      //    origin. This is a guard against deduping when www.site.com and
      //    site.com have different content.
      // 2. If the heuristic result is higher than either the www origin or
      //    non-www origin:
      //    2a. If the heuristic is a www origin, discard the non-www origin.
      //    2b. If the heuristic is a non-www origin, discard the www origin.
      if (
        !result.heuristic &&
        result.type == UrlbarUtils.RESULT_TYPE.URL &&
        result.payload.url
      ) {
        let [strippedUrl, prefix] = UrlbarUtils.stripPrefixAndTrim(
          result.payload.url,
          {
            stripHttp: true,
            stripHttps: true,
            stripWww: true,
            trimEmptyQuery: true,
          }
        );
        let topPrefixData = strippedUrlToTopPrefixAndTitle.get(strippedUrl);
        // We don't expect completely identical URLs in the results at this
        // point, so if the prefixes are the same, then we're deduping a result
        // against itself.
        if (topPrefixData && prefix != topPrefixData.prefix) {
          let prefixRank = UrlbarUtils.getPrefixRank(prefix);
          if (
            topPrefixData.rank > prefixRank &&
            prefix.endsWith("www.") == topPrefixData.prefix.endsWith("www.")
          ) {
            continue;
          } else if (
            topPrefixData.rank > prefixRank &&
            result.payload?.title == topPrefixData.title
          ) {
            continue;
          }
        }
      }

      // Exclude results that dupe autofill.
      if (
        context.heuristicResult &&
        context.heuristicResult.providerName == "Autofill" &&
        result.providerName != "Autofill" &&
        context.heuristicResult.payload?.url == result.payload.url &&
        context.heuristicResult.type == result.type
      ) {
        continue;
      }

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
        let submission = Services.search.parseSubmissionURL(result.payload.url);
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
