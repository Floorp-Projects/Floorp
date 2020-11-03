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
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
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
      if (result.payload.suggestion) {
        return UrlbarUtils.RESULT_GROUP.SUGGESTION;
      }
      break;
    case UrlbarUtils.RESULT_TYPE.OMNIBOX:
      return UrlbarUtils.RESULT_GROUP.EXTENSION;
  }
  return UrlbarUtils.RESULT_GROUP.GENERAL;
}

// Breaks ties among heuristic results. Providers higher up the list are higher
// priority.
const HEURISTIC_ORDER = [
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

  /**
   * Sorts results in the given UrlbarQueryContext.
   *
   * @param {UrlbarQueryContext} context
   *   The query context.
   */
  sort(context) {
    // This method is called multiple times per keystroke, so it should be as
    // fast and efficient as possible.  We do two passes through the results:
    // one to collect state for the second pass, and then a second to build the
    // sorted list of results.  If you find yourself writing something like
    // context.results.find(), filter(), sort(), etc., modify one or both passes
    // instead.

    // Global state we'll use to make decisions during this sort.
    let state = {
      context,
      resultsByGroup: new Map(),
      totalResultCount: 0,
      topHeuristicRank: Infinity,
      strippedUrlToTopPrefixAndTitle: new Map(),
      canShowPrivateSearch: context.results.length > 1,
      canShowTailSuggestions: true,
      formHistorySuggestions: new Set(),
      canAddTabToSearch: true,
    };

    let resultsWithSuggestedIndex = [];

    // Do the first pass over all results to build some state.
    for (let result of context.results) {
      // Save results that have a suggested index for later.
      if (result.suggestedIndex >= 0) {
        resultsWithSuggestedIndex.push(result);
        continue;
      }

      // Add all other results to the resultsByGroup map:
      // group => array of results belonging to the group
      let group = groupFromResult(result);
      let results = state.resultsByGroup.get(group);
      if (!results) {
        results = [];
        state.resultsByGroup.set(group, results);
      }
      results.push(result);

      // Update pre-add state.
      this._updateStatePreAdd(result, state);
    }

    if (
      context.heuristicResult?.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      context.heuristicResult?.payload.query
    ) {
      state.heuristicResultQuery = context.heuristicResult.payload.query.toLocaleLowerCase();
    }

    // If the heuristic result is a search result, use search buckets, otherwise
    // use normal buckets.
    let buckets =
      context.heuristicResult?.type == UrlbarUtils.RESULT_TYPE.SEARCH
        ? UrlbarPrefs.get("matchBucketsSearch")
        : UrlbarPrefs.get("matchBuckets");
    // In search mode for an engine, we always want to show search suggestions
    // before general results.
    if (context.searchMode?.engineName) {
      let suggestionsIndex = buckets.findIndex(b => b[0] == "suggestion");
      let generalIndex = buckets.findIndex(b => b[0] == "general");
      if (generalIndex < suggestionsIndex) {
        // Copy the array, otherwise we'd end up modifying the cached pref.
        buckets = buckets.slice();
        buckets[generalIndex] = "suggestion";
        buckets[suggestionsIndex] = "general";
      }
    }
    logger.debug(`Buckets: ${buckets}`);

    // Do the second pass to fill each bucket.  We'll build a list where each
    // item at index i is the array of results in the bucket at index i.
    let resultsByBucketIndex = [];
    for (let [group, maxResultCount] of buckets) {
      let results = this._addResults(group, maxResultCount, state);
      resultsByBucketIndex.push(results);
    }

    // Build the sorted results list by concatenating each bucket's results.
    let sortedResults = [];
    let remainingCount = context.maxResults;
    for (let i = 0; i < resultsByBucketIndex.length && remainingCount; i++) {
      let results = resultsByBucketIndex[i];
      let count = Math.min(remainingCount, results.length);
      sortedResults.push(...results.slice(0, count));
      remainingCount -= count;
    }

    // Finally, insert results that have a suggested index.  Sort them by index
    // in descending order so that earlier insertions don't disrupt later ones.
    resultsWithSuggestedIndex.sort(
      (a, b) => a.suggestedIndex - b.suggestedIndex
    );
    // Do a first pass to update sort state for each result.
    for (let result of resultsWithSuggestedIndex) {
      this._updateStatePreAdd(result, state);
    }
    // Now insert them.
    for (let result of resultsWithSuggestedIndex) {
      if (this._canAddResult(result, state)) {
        let index =
          result.suggestedIndex <= sortedResults.length
            ? result.suggestedIndex
            : sortedResults.length;
        sortedResults.splice(index, 0, result);
        this._updateStatePostAdd(result, state);
      }
    }

    context.results = sortedResults;
  }

  /**
   * Adds results to a bucket using results from the bucket's group in
   * `state.resultsByGroup`.
   *
   * @param {string} group
   *   The bucket's group.
   * @param {number} maxResultCount
   *   The maximum number of results to add to the bucket.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   * @returns {array}
   *   The added results, empty if no results were added.
   */
  _addResults(group, maxResultCount, state) {
    let addedResults = [];
    let groupResults = state.resultsByGroup.get(group);
    while (
      groupResults?.length &&
      addedResults.length < maxResultCount &&
      state.totalResultCount < state.context.maxResults
    ) {
      // We either add or discard results in the order they appear in the
      // groupResults array, so shift() them off.  That way later buckets with
      // the same group won't include results that earlier buckets have added or
      // discarded.
      let result = groupResults.shift();
      if (this._canAddResult(result, state)) {
        addedResults.push(result);
        state.totalResultCount++;
        this._updateStatePostAdd(result, state);
      }
    }
    return addedResults;
  }

  /**
   * Returns whether a result can be added to its bucket given the current sort
   * state.
   *
   * @param {UrlbarResult} result
   *   The result.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   * @returns {boolean}
   *   True if the result can be added and false if it should be discarded.
   */
  _canAddResult(result, state) {
    // Exclude low-ranked heuristic results.
    if (result.heuristic && result != state.context.heuristicResult) {
      return false;
    }

    // We expect UnifiedComplete sent us the highest-ranked www. and non-www
    // origins, if any. Now, compare them to each other and to the heuristic
    // result.
    //
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
      let topPrefixData = state.strippedUrlToTopPrefixAndTitle.get(strippedUrl);
      // We don't expect completely identical URLs in the results at this point,
      // so if the prefixes are the same, then we're deduping a result against
      // itself.
      if (topPrefixData && prefix != topPrefixData.prefix) {
        let prefixRank = UrlbarUtils.getPrefixRank(prefix);
        if (
          prefixRank < topPrefixData.rank &&
          (prefix.endsWith("www.") == topPrefixData.prefix.endsWith("www.") ||
            result.payload?.title == topPrefixData.title)
        ) {
          return false;
        }
      }
    }

    // Discard results that dupe autofill.
    if (
      state.context.heuristicResult &&
      state.context.heuristicResult.providerName == "Autofill" &&
      result.providerName != "Autofill" &&
      state.context.heuristicResult.payload?.url == result.payload.url &&
      state.context.heuristicResult.type == result.type
    ) {
      return false;
    }

    // HeuristicFallback may add non-heuristic results in some cases, but those
    // should be retained only if the heuristic result comes from it.
    if (
      !result.heuristic &&
      result.providerName == "HeuristicFallback" &&
      state.context.heuristicResult?.providerName != "HeuristicFallback"
    ) {
      return false;
    }

    if (result.providerName == "TabToSearch") {
      // Discard tab-to-search results if we're not autofilling a URL or
      // a tab-to-search result was added already.
      if (
        state.context.heuristicResult.type != UrlbarUtils.RESULT_TYPE.URL ||
        !state.context.heuristicResult.autofill ||
        !state.canAddTabToSearch
      ) {
        return false;
      }

      let autofillHostname = new URL(state.context.heuristicResult.payload.url)
        .hostname;
      let [autofillDomain] = UrlbarUtils.stripPrefixAndTrim(autofillHostname, {
        stripWww: true,
      });
      // Strip the public suffix because we want to allow matching "domain.it"
      // with "domain.com".
      autofillDomain = UrlbarUtils.stripPublicSuffixFromHost(autofillDomain);
      if (!autofillDomain) {
        return false;
      }

      // For tab-to-search results, result.payload.url is the engine's domain
      // with the public suffix already stripped, for example "www.mozilla.".
      let [engineDomain] = UrlbarUtils.stripPrefixAndTrim(result.payload.url, {
        stripWww: true,
      });
      // Discard if the engine domain does not end with the autofilled one.
      if (!engineDomain.endsWith(autofillDomain)) {
        return false;
      }
    }

    // Discard "Search in a Private Window" if appropriate.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.inPrivateWindow &&
      !state.canShowPrivateSearch
    ) {
      return false;
    }

    // Discard form history that dupes the heuristic or previous added form
    // history (for restyleSearch).
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.source == UrlbarUtils.RESULT_SOURCE.HISTORY &&
      (result.payload.lowerCaseSuggestion === state.heuristicResultQuery ||
        state.formHistorySuggestions.has(result.payload.lowerCaseSuggestion))
    ) {
      return false;
    }

    // Discard remote search suggestions that dupe the heuristic.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.source == UrlbarUtils.RESULT_SOURCE.SEARCH &&
      result.payload.lowerCaseSuggestion &&
      result.payload.lowerCaseSuggestion === state.heuristicResultQuery
    ) {
      return false;
    }

    // Discard tail suggestions if appropriate.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.tail &&
      !state.canShowTailSuggestions
    ) {
      return false;
    }

    // Discard SERPs from browser history that dupe either the heuristic or
    // previously added form history.
    if (
      result.source == UrlbarUtils.RESULT_SOURCE.HISTORY &&
      result.type == UrlbarUtils.RESULT_TYPE.URL
    ) {
      let submission = Services.search.parseSubmissionURL(result.payload.url);
      if (submission) {
        let resultQuery = submission.terms.toLocaleLowerCase();
        if (
          state.heuristicResultQuery === resultQuery ||
          state.formHistorySuggestions.has(resultQuery)
        ) {
          // If the result's URL is the same as a brand new SERP URL created
          // from the query string modulo certain URL params, then treat the
          // result as a dupe and discard it.
          let [newSerpURL] = UrlbarUtils.getSearchQueryUrl(
            submission.engine,
            resultQuery
          );
          if (
            UrlbarSearchUtils.serpsAreEquivalent(result.payload.url, newSerpURL)
          ) {
            return false;
          }
        }
      }
    }

    // Include the result.
    return true;
  }

  /**
   * Updates the global state that we use to make decisions during sort.  This
   * should be called for results before we've decided whether to add or discard
   * them.
   *
   * @param {UrlbarResult} result
   *   The result.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   */
  _updateStatePreAdd(result, state) {
    // Determine the highest-ranking heuristic result.
    if (result.heuristic) {
      // + 2 to reserve the highest-priority slots for test and extension
      // providers.
      let heuristicRank = HEURISTIC_ORDER.indexOf(result.providerName) + 2;
      // Extension and test provider names vary widely and aren't suitable
      // for a static safelist like HEURISTIC_ORDER.
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
      if (heuristicRank <= state.topHeuristicRank) {
        state.topHeuristicRank = heuristicRank;
        state.context.heuristicResult = result;
      }
    }

    // Save some state we'll use later to dedupe URL results.
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
      let topPrefixData = state.strippedUrlToTopPrefixAndTitle.get(strippedUrl);
      let topPrefixRank = topPrefixData ? topPrefixData.rank : -1;
      if (topPrefixRank < prefixRank) {
        // strippedUrl => { prefix, title, rank }
        state.strippedUrlToTopPrefixAndTitle.set(strippedUrl, {
          prefix,
          title: result.payload.title,
          rank: prefixRank,
        });
      }
    }

    // If we find results other than the heuristic, "Search in Private
    // Window," or tail suggestions, then we should hide tail suggestions
    // since they're a last resort.
    if (
      state.canShowTailSuggestions &&
      !result.heuristic &&
      (result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        (!result.payload.inPrivateWindow && !result.payload.tail))
    ) {
      state.canShowTailSuggestions = false;
    }
  }

  /**
   * Updates the global state that we use to make decisions during sort.  This
   * should be called for results after they've been added.  It should not be
   * called for discarded results.
   *
   * @param {UrlbarResult} result
   *   The result.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   */
  _updateStatePostAdd(result, state) {
    // The "Search in a Private Window" result should only be shown when there
    // are other results and all of them are searches.  It should not be shown
    // if the user typed an alias because that's an explicit engine choice.
    if (
      state.canShowPrivateSearch &&
      (result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        result.payload.keywordOffer ||
        (result.heuristic && result.payload.keyword))
    ) {
      state.canShowPrivateSearch = false;
    }

    // Update form history suggestions.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.source == UrlbarUtils.RESULT_SOURCE.HISTORY
    ) {
      state.formHistorySuggestions.add(result.payload.lowerCaseSuggestion);
    }

    // Avoid multiple tab-to-search results.
    // TODO (Bug 1670185): figure out better strategies to manage this case.
    if (result.providerName == "TabToSearch") {
      state.canAddTabToSearch = false;
    }
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
