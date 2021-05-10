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
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.jsm",
  UrlbarMuxer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  UrlbarUtils.getLogger({ prefix: "MuxerUnifiedComplete" })
);

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
      strippedUrlToTopPrefixAndTitle: new Map(),
      canShowPrivateSearch: context.results.length > 1,
      canShowTailSuggestions: true,
      // Form history and remote suggestions added so far.  Used for deduping
      // suggestions.  Also includes the heuristic query string if the heuristic
      // is a search result.  All strings in the set are lowercased.
      suggestions: new Set(),
      canAddTabToSearch: true,
      // When you add state, update _copyState() as necessary.
    };

    // Do the first pass over all results to build some state.
    for (let result of context.results) {
      if (result.providerName == "UrlbarProviderQuickSuggest") {
        // Quick suggest results are handled specially and always come at the
        // end of the general bucket.
        // TODO (Bug 1710518): Come up with a more general solution.
        state.quickSuggestResult = result;
        this._updateStatePreAdd(result, state);
        continue;
      }

      // Add each result to the resultsByGroup map:
      // group => array of results belonging to the group
      let group = UrlbarUtils.getResultGroup(result);
      let results = state.resultsByGroup.get(group);
      if (!results) {
        results = [];
        state.resultsByGroup.set(group, results);
      }
      results.push(result);

      // Update pre-add state.
      this._updateStatePreAdd(result, state);
    }

    // Determine the buckets to use for this sort.  In search mode with an
    // engine, show search suggestions first.
    let rootBucket = context.searchMode?.engineName
      ? UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: true })
      : UrlbarPrefs.get("resultBuckets");
    logger.debug(`Buckets: ${rootBucket}`);

    let sortedResults = this._fillBuckets(
      rootBucket,
      context.maxResults,
      state
    );

    this._addSuggestedIndexResults(sortedResults, state);

    this._truncateResults(sortedResults, context.maxResults);
    context.results = sortedResults;
  }

  /**
   * Returns a *deep* copy of state (except for `state.context`, which is
   * shallow copied).  i.e., any Maps, Sets, and arrays in the state should be
   * recursively copied so that the original state is not modified when the copy
   * is modified.
   *
   * @param {object} state
   *   The muxer state to copy.
   * @returns {object}
   *   A deep copy of the state.
   */
  _copyState(state) {
    let copy = Object.assign({}, state, {
      resultsByGroup: new Map(),
      strippedUrlToTopPrefixAndTitle: new Map(
        state.strippedUrlToTopPrefixAndTitle
      ),
      suggestions: new Set(state.suggestions),
    });
    for (let [group, results] of state.resultsByGroup) {
      copy.resultsByGroup.set(group, [...results]);
    }
    return copy;
  }

  /**
   * Recursively fills a result bucket.
   *
   * @param {object} bucket
   *   The result bucket to fill.
   * @param {number} maxResultCount
   *   The maximum number of results to include in the bucket.
   * @param {object} state
   *   The muxer state.
   * @returns {array}
   *   A flat array of results in the bucket.
   */
  _fillBuckets(bucket, maxResultCount, state) {
    // If there are no child buckets, then fill the bucket directly.
    if (!bucket.children) {
      return this._addResults(bucket.group, maxResultCount, state);
    }

    // Set up some flex state for the bucket.
    let stateCopy;
    let flexSum = 0;
    let unfilledChildIndexes = [];
    let unfilledChildResultCount = 0;
    if (bucket.flexChildren) {
      stateCopy = this._copyState(state);
      for (let child of bucket.children) {
        let flex = typeof child.flex == "number" ? child.flex : 0;
        flexSum += flex;
      }
    }

    // Sum of child bucket flex values for children that could be completely
    // filled.
    let flexSumFilled = flexSum;

    // Fill each child bucket, collecting all results in `results`.
    let results = [];
    for (
      let i = 0;
      i < bucket.children.length && results.length < maxResultCount;
      i++
    ) {
      let child = bucket.children[i];

      // Compute the child's max result count.
      let childMaxResultCount;
      if (bucket.flexChildren) {
        let flex = typeof child.flex == "number" ? child.flex : 0;
        childMaxResultCount = Math.round(maxResultCount * (flex / flexSum));
      } else {
        childMaxResultCount = Math.min(
          typeof child.maxResultCount == "number"
            ? child.maxResultCount
            : Infinity,
          // parent max result count - current total of child results
          maxResultCount - results.length
        );
      }

      // Recurse and fill the child bucket.
      let childResults = this._fillBuckets(child, childMaxResultCount, state);
      results = results.concat(childResults);

      if (bucket.flexChildren && childResults.length < childMaxResultCount) {
        // The flexed child bucket wasn't completely filled.  We'll try to make
        // up the difference below by overfilling children that did fill up.
        let flex = typeof child.flex == "number" ? child.flex : 0;
        flexSumFilled -= flex;
        unfilledChildIndexes.push(i);
        unfilledChildResultCount += childResults.length;
      }
    }

    // If the child buckets are flexed and some didn't fill up, then discard the
    // results and do one more pass, trying to recursively overfill child
    // buckets that did fill up while still respecting their flex ratios.
    if (unfilledChildIndexes.length) {
      results = [];
      let remainingResultCount = maxResultCount - unfilledChildResultCount;
      for (
        let i = 0;
        i < bucket.children.length && results.length < maxResultCount;
        i++
      ) {
        let child = bucket.children[i];
        let childMaxResultCount;
        if (unfilledChildIndexes.length && i == unfilledChildIndexes[0]) {
          // This is one of the children that didn't fill up.  Since it didn't
          // fill up, the max result count to use in this pass isn't important
          // as long as it's >= the number of results it was able to fill.  We
          // can't re-use its results from the first pass (even though they're
          // still correct) because we need to properly update `stateCopy` and
          // therefore re-fill the child.
          unfilledChildIndexes.shift();
          childMaxResultCount = maxResultCount;
        } else {
          let flex = typeof child.flex == "number" ? child.flex : 0;
          childMaxResultCount = flex
            ? Math.round(remainingResultCount * (flex / flexSumFilled))
            : remainingResultCount;
        }
        let childResults = this._fillBuckets(
          child,
          childMaxResultCount,
          stateCopy
        );
        results = results.concat(childResults);
      }

      // Update `state` in place so that it's also updated in the caller.
      for (let [key, value] of Object.entries(stateCopy)) {
        state[key] = value;
      }
    }

    return results;
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
    // For form history, maxHistoricalSearchSuggestions == 0 implies that the
    // user has opted out of form history completely, so we override maxResult
    // count here in that case.  Other values of maxHistoricalSearchSuggestions
    // are ignored and we use the flex defined on the form history bucket.
    if (
      group == UrlbarUtils.RESULT_GROUP.FORM_HISTORY &&
      !UrlbarPrefs.get("maxHistoricalSearchSuggestions")
    ) {
      maxResultCount = 0;
    }

    let addQuickSuggest =
      state.quickSuggestResult &&
      group == UrlbarUtils.RESULT_GROUP.GENERAL &&
      this._canAddResult(state.quickSuggestResult, state);
    if (addQuickSuggest) {
      maxResultCount--;
    }

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

    if (addQuickSuggest) {
      let { quickSuggestResult } = state;
      state.quickSuggestResult = null;
      addedResults.push(quickSuggestResult);
      state.totalResultCount++;
      this._updateStatePostAdd(quickSuggestResult, state);
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
      // Discard the result if a tab-to-search result was added already.
      if (!state.canAddTabToSearch) {
        return false;
      }

      if (!result.payload.satisfiesAutofillThreshold) {
        // Discard the result if the heuristic result is not autofill.
        if (
          !state.context.heuristicResult ||
          state.context.heuristicResult.type != UrlbarUtils.RESULT_TYPE.URL ||
          !state.context.heuristicResult.autofill
        ) {
          return false;
        }

        let autofillHostname = new URL(
          state.context.heuristicResult.payload.url
        ).hostname;
        let [autofillDomain] = UrlbarUtils.stripPrefixAndTrim(
          autofillHostname,
          {
            stripWww: true,
          }
        );
        // Strip the public suffix because we want to allow matching "domain.it"
        // with "domain.com".
        autofillDomain = UrlbarUtils.stripPublicSuffixFromHost(autofillDomain);
        if (!autofillDomain) {
          return false;
        }

        // For tab-to-search results, result.payload.url is the engine's domain
        // with the public suffix already stripped, for example "www.mozilla.".
        let [engineDomain] = UrlbarUtils.stripPrefixAndTrim(
          result.payload.url,
          {
            stripWww: true,
          }
        );
        // Discard if the engine domain does not end with the autofilled one.
        if (!engineDomain.endsWith(autofillDomain)) {
          return false;
        }
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

    // Discard form history and remote suggestions that dupe previously added
    // suggestions or the heuristic.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.lowerCaseSuggestion
    ) {
      let suggestion = result.payload.lowerCaseSuggestion.trim();
      if (!suggestion || state.suggestions.has(suggestion)) {
        return false;
      }
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
    // previously added suggestions.
    if (
      result.source == UrlbarUtils.RESULT_SOURCE.HISTORY &&
      result.type == UrlbarUtils.RESULT_TYPE.URL
    ) {
      let submission = Services.search.parseSubmissionURL(result.payload.url);
      if (submission) {
        let resultQuery = submission.terms.trim().toLocaleLowerCase();
        if (state.suggestions.has(resultQuery)) {
          // If the result's URL is the same as a brand new SERP URL created
          // from the query string modulo certain URL params, then treat the
          // result as a dupe and discard it.
          let [newSerpURL] = UrlbarUtils.getSearchQueryUrl(
            submission.engine,
            submission.terms
          );
          if (
            UrlbarSearchUtils.serpsAreEquivalent(result.payload.url, newSerpURL)
          ) {
            return false;
          }
        }
      }
    }

    // When in an engine search mode, discard URL results whose hostnames don't
    // include the root domain of the search mode engine.
    if (state.context.searchMode?.engineName && result.payload.url) {
      let engine = Services.search.getEngineByName(
        state.context.searchMode.engineName
      );
      if (engine) {
        let searchModeRootDomain = UrlbarSearchUtils.getRootDomainFromEngine(
          engine
        );
        let resultUrl = new URL(result.payload.url);
        // Add a trailing "." to increase the stringency of the check. This
        // check covers most general cases. Some edge cases are not covered,
        // like `resultUrl` being ebay.mydomain.com, which would escape this
        // check if `searchModeRootDomain` was "ebay".
        if (!resultUrl.hostname.includes(`${searchModeRootDomain}.`)) {
          return false;
        }
      }
    }

    // Heuristic results must always be the first result.  If this result is a
    // heuristic but we've already added results, discard it.  Normally this
    // should never happen because the standard result buckets are set up so
    // that there's always at most one heuristic and it's always first, but
    // since result buckets are stored in a pref and can therefore be modified
    // by the user, we perform this check.
    if (result.heuristic && state.totalResultCount) {
      return false;
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
    // Update heuristic state.
    if (result.heuristic) {
      state.context.heuristicResult = result;
      if (
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.payload.query
      ) {
        let query = result.payload.query.trim().toLocaleLowerCase();
        if (query) {
          state.suggestions.add(query);
        }
      }
    }

    // The "Search in a Private Window" result should only be shown when there
    // are other results and all of them are searches.  It should not be shown
    // if the user typed an alias because that's an explicit engine choice.
    if (
      state.canShowPrivateSearch &&
      (result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        result.payload.providesSearchMode ||
        (result.heuristic && result.payload.keyword))
    ) {
      state.canShowPrivateSearch = false;
    }

    // Update suggestions.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.lowerCaseSuggestion
    ) {
      let suggestion = result.payload.lowerCaseSuggestion.trim();
      if (suggestion) {
        state.suggestions.add(suggestion);
      }
    }

    // Avoid multiple tab-to-search results.
    // TODO (Bug 1670185): figure out better strategies to manage this case.
    if (result.providerName == "TabToSearch") {
      state.canAddTabToSearch = false;
      // We want to record in urlbar.tips once per engagement per engine. Since
      // whether these results are shown is dependent on the Muxer, we must
      // add to `enginesShown` here.
      if (result.payload.dynamicType) {
        UrlbarProviderTabToSearch.enginesShown.onboarding.add(
          result.payload.engine
        );
      } else {
        UrlbarProviderTabToSearch.enginesShown.regular.add(
          result.payload.engine
        );
      }
    }
  }

  /**
   * Inserts results with suggested indexes.  This should be called at the end
   * of the sort, after all buckets have been filled.
   *
   * @param {array} sortedResults
   *   The sorted results produced by the muxer so far.  Updated in place.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   */
  _addSuggestedIndexResults(sortedResults, state) {
    let suggestedIndexResults = state.resultsByGroup.get(
      UrlbarUtils.RESULT_GROUP.SUGGESTED_INDEX
    );
    if (!suggestedIndexResults) {
      return;
    }

    // First, sort the results by index in ascending order so that insertions of
    // results with both positive and negative indexes are in ascending order.
    suggestedIndexResults.sort((a, b) => a.suggestedIndex - b.suggestedIndex);

    // Insert results with positive indexes.  Insertions should happen in
    // ascending order so that higher-index results are inserted at their
    // suggested indexes and aren't offset by later lower-index insertions.
    let negativeIndexSpanCount = 0;
    for (let result of suggestedIndexResults) {
      if (result.suggestedIndex < 0) {
        negativeIndexSpanCount += UrlbarUtils.getSpanForResult(result);
      } else {
        this._updateStatePreAdd(result, state);
        if (this._canAddResult(result, state)) {
          let index =
            result.suggestedIndex <= sortedResults.length
              ? result.suggestedIndex
              : sortedResults.length;
          sortedResults.splice(index, 0, result);
          this._updateStatePostAdd(result, state);
        }
      }
    }

    // Before inserting results with negative indexes, truncate the sorted
    // results so that their total span count is no larger than maxResults minus
    // the span count of the negative-index results themselves.  If we didn't do
    // that, the negative-index results could end up getting removed when the
    // muxer truncates the final results array, which would effectively mean
    // that we inserted them at the wrong indexes.
    this._truncateResults(
      sortedResults,
      state.context.maxResults - negativeIndexSpanCount
    );

    // Insert results with negative indexes.
    if (negativeIndexSpanCount) {
      for (let result of suggestedIndexResults) {
        if (result.suggestedIndex >= 0) {
          break;
        }
        this._updateStatePreAdd(result, state);
        if (this._canAddResult(result, state)) {
          let index = Math.max(
            result.suggestedIndex + sortedResults.length + 1,
            0
          );
          sortedResults.splice(index, 0, result);
          this._updateStatePostAdd(result, state);
        }
      }
    }
  }

  /**
   * Truncates the array of results so that their total span count is no larger
   * than a given number.
   *
   * @param {array} sortedResults
   *   The sorted results produced by the muxer so far.  Updated in place.
   * @param {number} maxSpanCount
   *   The max span count.
   */
  _truncateResults(sortedResults, maxSpanCount) {
    let remainingSpanCount = maxSpanCount;
    for (let i = 0; i < sortedResults.length; i++) {
      remainingSpanCount -= UrlbarUtils.getSpanForResult(sortedResults[i]);
      if (remainingSpanCount < 0) {
        sortedResults.splice(i, sortedResults.length - i);
        break;
      }
    }
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
