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
      // This is analogous to `maxResults` except it's the total available
      // result span instead of the total available result count. We'll add
      // results until `usedResultSpan` would exceed `availableResultSpan`.
      availableResultSpan: context.maxResults,
      // The total span of results that have been added so far.
      usedResultSpan: 0,
      strippedUrlToTopPrefixAndTitle: new Map(),
      urlToTabResultType: new Map(),
      addedRemoteTabUrls: new Set(),
      canShowPrivateSearch: context.results.length > 1,
      canShowTailSuggestions: true,
      // Form history and remote suggestions added so far.  Used for deduping
      // suggestions.  Also includes the heuristic query string if the heuristic
      // is a search result.  All strings in the set are lowercased.
      suggestions: new Set(),
      canAddTabToSearch: true,
      hasUnitConversionResult: false,
      // When you add state, update _copyState() as necessary.
    };

    // Do the first pass over all results to build some state.
    for (let result of context.results) {
      if (result.providerName == "UrlbarProviderQuickSuggest") {
        // Quick suggest results are handled specially and are inserted at a
        // Nimbus-configurable position within the general bucket.
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

    // Subtract from `availableResultSpan` the total span of suggestedIndex
    // results so there will be room for them at the end of the sort.
    let suggestedIndexResults = state.resultsByGroup.get(
      UrlbarUtils.RESULT_GROUP.SUGGESTED_INDEX
    );
    if (suggestedIndexResults) {
      let span = suggestedIndexResults.reduce((sum, result) => {
        if (this._canAddResult(result, state)) {
          sum += UrlbarUtils.getSpanForResult(result);
        }
        return sum;
      }, 0);
      state.availableResultSpan = Math.max(state.availableResultSpan - span, 0);
    }

    // Determine the buckets to use for this sort.  In search mode with an
    // engine, show search suggestions first.
    let rootBucket = context.searchMode?.engineName
      ? UrlbarPrefs.makeResultBuckets({ showSearchSuggestionsFirst: true })
      : UrlbarPrefs.get("resultBuckets");
    logger.debug(`Buckets: ${rootBucket}`);

    let [sortedResults] = this._fillBuckets(
      rootBucket,
      state.availableResultSpan,
      state
    );

    this._addSuggestedIndexResults(sortedResults, state);

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
      urlToTabResultType: new Map(state.urlToTabResultType),
      addedRemoteTabUrls: new Set(state.addedRemoteTabUrls),
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
   * @param {number} availableSpan
   *   The maximum total result span to include in the bucket.
   * @param {object} state
   *   The muxer state.
   * @returns {array}
   *   `[results, span]`, where `results` is a flat array of results in the
   *   bucket and `span` is the total span of the results.
   */
  _fillBuckets(bucket, availableSpan, state) {
    // If there are no child buckets, then fill the bucket directly.
    if (!bucket.children) {
      return this._addResults(bucket.group, availableSpan, state);
    }

    // Set up some flex state for the bucket.
    let stateCopy;
    let flexSum = 0;
    let unfilledChildIndexes = [];
    let unfilledChildSpan = 0;
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
    let usedSpan = 0;
    for (
      let i = 0;
      i < bucket.children.length && usedSpan < availableSpan;
      i++
    ) {
      let child = bucket.children[i];

      // Compute the child's total available result span.
      let availableChildSpan;
      if (bucket.flexChildren) {
        let flex = typeof child.flex == "number" ? child.flex : 0;
        availableChildSpan = Math.round(availableSpan * (flex / flexSum));
      } else {
        availableChildSpan = Math.min(
          typeof child.maxResultCount == "number"
            ? child.maxResultCount
            : Infinity,
          availableSpan - usedSpan
        );
      }

      // Recurse and fill the child bucket.
      let [childResults, usedChildSpan] = this._fillBuckets(
        child,
        availableChildSpan,
        state
      );
      results = results.concat(childResults);
      usedSpan += usedChildSpan;

      if (bucket.flexChildren && usedChildSpan < availableChildSpan) {
        // The flexed child bucket wasn't completely filled.  We'll try to make
        // up the difference below by overfilling children that did fill up.
        let flex = typeof child.flex == "number" ? child.flex : 0;
        flexSumFilled -= flex;
        unfilledChildIndexes.push(i);
        unfilledChildSpan += usedChildSpan;
      }
    }

    // If the child buckets are flexed and some didn't fill up, then discard the
    // results and do one more pass, trying to recursively overfill child
    // buckets that did fill up while still respecting their flex ratios.
    if (unfilledChildIndexes.length) {
      results = [];
      usedSpan = 0;
      let remainingSpan = availableSpan - unfilledChildSpan;
      for (
        let i = 0;
        i < bucket.children.length && usedSpan < availableSpan;
        i++
      ) {
        let child = bucket.children[i];
        let availableChildSpan;
        if (unfilledChildIndexes.length && i == unfilledChildIndexes[0]) {
          // This is one of the children that didn't fill up. Since it didn't
          // fill up, the available span to use in this pass isn't important as
          // long as it's >= the span that was filled. We can't re-use its
          // results from the first pass (even though they're still correct)
          // because we need to properly update `stateCopy` and therefore
          // re-fill the child.
          unfilledChildIndexes.shift();
          availableChildSpan = availableSpan;
        } else {
          let flex = typeof child.flex == "number" ? child.flex : 0;
          availableChildSpan = flex
            ? Math.round(remainingSpan * (flex / flexSumFilled))
            : remainingSpan;
        }
        let [childResults, usedChildSpan] = this._fillBuckets(
          child,
          availableChildSpan,
          stateCopy
        );
        results = results.concat(childResults);
        usedSpan += usedChildSpan;
      }

      // Update `state` in place so that it's also updated in the caller.
      for (let [key, value] of Object.entries(stateCopy)) {
        state[key] = value;
      }
    }

    return [results, usedSpan];
  }

  /**
   * Adds results to a bucket using results from the bucket's group in
   * `state.resultsByGroup`.
   *
   * @param {string} group
   *   The bucket's group.
   * @param {number} availableSpan
   *   The maximum total result span to add to the bucket.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   * @returns {array}
   *   `[results, span]`, where `results` is the array of added results and
   *   `span` is the total span of the results. If no results were added,
   *   `results` will be empty and `span` will be zero.
   */
  _addResults(group, availableSpan, state) {
    // For form history, maxHistoricalSearchSuggestions == 0 implies the user
    // has opted out of form history completely, so we override the available
    // span here in that case. Other values of maxHistoricalSearchSuggestions
    // are ignored and we use the flex defined on the form history bucket.
    if (
      group == UrlbarUtils.RESULT_GROUP.FORM_HISTORY &&
      !UrlbarPrefs.get("maxHistoricalSearchSuggestions")
    ) {
      availableSpan = 0;
    }

    let addQuickSuggest =
      state.quickSuggestResult &&
      group == UrlbarUtils.RESULT_GROUP.GENERAL &&
      this._canAddResult(state.quickSuggestResult, state);
    if (addQuickSuggest) {
      availableSpan -= UrlbarUtils.getSpanForResult(state.quickSuggestResult);
    }

    let addedResults = [];
    let usedSpan = 0;
    let groupResults = state.resultsByGroup.get(group);
    while (
      groupResults?.length &&
      usedSpan < availableSpan &&
      state.usedResultSpan < state.availableResultSpan
    ) {
      let result = groupResults[0];
      if (this._canAddResult(result, state)) {
        let span = UrlbarUtils.getSpanForResult(result);
        let newUsedSpan = usedSpan + span;
        if (availableSpan < newUsedSpan && !result.heuristic) {
          // Adding the result would exceed the bucket's available span, so stop
          // adding results to it. Skip the shift() below so the result can be
          // added to later buckets. The heuristic result is an exception since
          // we should always show it when it exists.
          break;
        }
        addedResults.push(result);
        usedSpan = newUsedSpan;
        state.usedResultSpan += span;
        this._updateStatePostAdd(result, state);
      }

      // We either add or discard results in the order they appear in
      // `groupResults`, so shift() them off. That way later buckets with the
      // same group won't include results that earlier buckets have added or
      // discarded.
      groupResults.shift();
    }

    if (addQuickSuggest) {
      let { quickSuggestResult } = state;
      state.quickSuggestResult = null;
      // Determine the index within the general bucket to insert the quick
      // suggest result. There are separate Nimbus variables for the sponsored
      // and non-sponsored types. If `payload.sponsoredText` is defined, then
      // the result is a non-sponsored type; otherwise it's a sponsored type.
      // (Yes, correct -- the name of that payload property is misleading, so
      // just ignore it for now. See bug 1695302.)
      let index = UrlbarPrefs.get(
        quickSuggestResult.payload.sponsoredText
          ? "quickSuggestNonSponsoredIndex"
          : "quickSuggestSponsoredIndex"
      );
      // A positive index is relative to the start of the bucket and a negative
      // index is relative to the end, similar to `suggestedIndex`.
      if (index < 0) {
        index = Math.max(index + addedResults.length + 1, 0);
      } else {
        index = Math.min(index, addedResults.length);
      }
      addedResults.splice(index, 0, quickSuggestResult);
      let span = UrlbarUtils.getSpanForResult(quickSuggestResult);
      usedSpan += span;
      state.usedResultSpan += span;
      this._updateStatePostAdd(quickSuggestResult, state);
    }

    return [addedResults, usedSpan];
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
      // If the condition below is not met, we are deduping a result against
      // itself.
      if (
        topPrefixData &&
        (prefix != topPrefixData.prefix ||
          result.providerName != topPrefixData.providerName)
      ) {
        let prefixRank = UrlbarUtils.getPrefixRank(prefix);
        if (
          (prefixRank < topPrefixData.rank &&
            (prefix.endsWith("www.") == topPrefixData.prefix.endsWith("www.") ||
              result.payload?.title == topPrefixData.title)) ||
          (prefix == topPrefixData.prefix &&
            result.providerName != topPrefixData.providerName)
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

    // Discard remote tab results that dupes another remote tab or a
    // switch-to-tab result.
    if (result.type == UrlbarUtils.RESULT_TYPE.REMOTE_TAB) {
      if (state.addedRemoteTabUrls.has(result.payload.url)) {
        return false;
      }
      let maybeDupeType = state.urlToTabResultType.get(result.payload.url);
      if (maybeDupeType == UrlbarUtils.RESULT_TYPE.TAB_SWITCH) {
        return false;
      }
    }

    // Discard history results that dupe either remote or switch-to-tab results.
    if (
      !result.heuristic &&
      result.type == UrlbarUtils.RESULT_TYPE.URL &&
      result.payload.url &&
      state.urlToTabResultType.has(result.payload.url)
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
    if (result.heuristic && state.usedResultSpan) {
      return false;
    }

    // Google search engine might suggest a result for unit conversion with
    // format that starts with "= ". If our UnitConversion can provide the
    // result, we discard the suggestion of Google in order to deduplicate.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.engine == "Google" &&
      result.payload.suggestion?.startsWith("= ") &&
      state.hasUnitConversionResult
    ) {
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
        // strippedUrl => { prefix, title, rank, providerName }
        state.strippedUrlToTopPrefixAndTitle.set(strippedUrl, {
          prefix,
          title: result.payload.title,
          rank: prefixRank,
          providerName: result.providerName,
        });
      }
    }

    // Save some state we'll use later to dedupe results from open/remote tabs.
    if (
      result.payload.url &&
      (result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH ||
        (result.type == UrlbarUtils.RESULT_TYPE.REMOTE_TAB &&
          !state.urlToTabResultType.has(result.payload.url)))
    ) {
      // url => result type
      state.urlToTabResultType.set(result.payload.url, result.type);
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

    state.hasUnitConversionResult =
      state.hasUnitConversionResult || result.providerName == "UnitConversion";
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

    // Sync will send us duplicate remote tabs if multiple copies of a tab are
    // open on a synced client. Keep track of which remote tabs we've added to
    // dedupe these.
    if (result.type == UrlbarUtils.RESULT_TYPE.REMOTE_TAB) {
      state.addedRemoteTabUrls.add(result.payload.url);
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

    // Partition the results into positive- and negative-index arrays. Positive
    // indexes are relative to the start of the list and negative indexes are
    // relative to the end.
    let positive = [];
    let negative = [];
    for (let result of suggestedIndexResults) {
      let results = result.suggestedIndex < 0 ? negative : positive;
      results.push(result);
    }

    // Sort the positive results ascending so that results at the end of the
    // array don't end up offset by later insertions at the front.
    positive.sort((a, b) => a.suggestedIndex - b.suggestedIndex);

    // Conversely, sort the negative results descending so that results at the
    // front of the array don't end up offset by later insertions at the end.
    negative.sort((a, b) => b.suggestedIndex - a.suggestedIndex);

    // Insert the results. We start with the positive results because we have
    // tests that assume they're inserted first. In practice it shouldn't matter
    // because there's no good reason we would ever have a negative result come
    // before a positive result in the same query. Even if we did, we have to
    // insert one before the other, and there's no right or wrong order.
    for (let results of [positive, negative]) {
      for (let result of results) {
        this._updateStatePreAdd(result, state);
        if (this._canAddResult(result, state)) {
          let index =
            result.suggestedIndex >= 0
              ? Math.min(result.suggestedIndex, sortedResults.length)
              : Math.max(result.suggestedIndex + sortedResults.length + 1, 0);
          sortedResults.splice(index, 0, result);
          this._updateStatePostAdd(result, state);
        }
      }
    }
  }
}

var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
