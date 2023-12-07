/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a component used to sort results in a UrlbarQueryContext.
 */

import {
  UrlbarMuxer,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.sys.mjs",
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  UrlbarUtils.getLogger({ prefix: "MuxerUnifiedComplete" })
);

/**
 * Constructs the map key by joining the url with the userContextId if
 * 'browser.urlbar.switchTabs.searchAllContainers' is set to true.
 * Otherwise, just the url is used.
 *
 * @param   {UrlbarResult} result The result object.
 * @returns {string} map key
 */
function makeMapKeyForTabResult(result) {
  return UrlbarUtils.tupleString(
    result.payload.url,
    lazy.UrlbarPrefs.get("switchTabs.searchAllContainers") &&
      result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH
      ? result.payload.userContextId
      : undefined
  );
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
   * @param {Array} unsortedResults
   *   The array of UrlbarResult that is not sorted yet.
   */
  sort(context, unsortedResults) {
    // This method is called multiple times per keystroke, so it should be as
    // fast and efficient as possible.  We do two passes through the results:
    // one to collect state for the second pass, and then a second to build the
    // sorted list of results.  If you find yourself writing something like
    // context.results.find(), filter(), sort(), etc., modify one or both passes
    // instead.

    // Global state we'll use to make decisions during this sort.
    let state = {
      context,
      // RESULT_GROUP => array of results belonging to the group, excluding
      // group-relative suggestedIndex results
      resultsByGroup: new Map(),
      // RESULT_GROUP => array of group-relative suggestedIndex results
      // belonging to the group
      suggestedIndexResultsByGroup: new Map(),
      // This is analogous to `maxResults` except it's the total available
      // result span instead of the total available result count. We'll add
      // results until `usedResultSpan` would exceed `availableResultSpan`.
      availableResultSpan: context.maxResults,
      // The total result span taken up by all global (non-group-relative)
      // suggestedIndex results.
      globalSuggestedIndexResultSpan: 0,
      // The total span of results that have been added so far.
      usedResultSpan: 0,
      strippedUrlToTopPrefixAndTitle: new Map(),
      urlToTabResultType: new Map(),
      addedRemoteTabUrls: new Set(),
      addedSwitchTabUrls: new Set(),
      addedResultUrls: new Set(),
      canShowPrivateSearch: unsortedResults.length > 1,
      canShowTailSuggestions: true,
      // Form history and remote suggestions added so far.  Used for deduping
      // suggestions.  Also includes the heuristic query string if the heuristic
      // is a search result.  All strings in the set are lowercased.
      suggestions: new Set(),
      canAddTabToSearch: true,
      hasUnitConversionResult: false,
      maxHeuristicResultSpan: 0,
      maxTabToSearchResultSpan: 0,
      // When you add state, update _copyState() as necessary.
    };

    // Do the first pass over all results to build some state.
    for (let result of unsortedResults) {
      // Add each result to the appropriate `resultsByGroup` map.
      let group = UrlbarUtils.getResultGroup(result);
      let resultsByGroup =
        result.hasSuggestedIndex && result.isSuggestedIndexRelativeToGroup
          ? state.suggestedIndexResultsByGroup
          : state.resultsByGroup;
      let results = resultsByGroup.get(group);
      if (!results) {
        results = [];
        resultsByGroup.set(group, results);
      }
      results.push(result);

      // Update pre-add state.
      this._updateStatePreAdd(result, state);
    }

    // Now that the first pass is done, adjust the available result span. More
    // than one tab-to-search result may be present but only one will be shown;
    // add the max TTS span to the total span of global suggestedIndex results.
    state.globalSuggestedIndexResultSpan += state.maxTabToSearchResultSpan;

    // Leave room for global suggestedIndex results at the end of the sort, by
    // subtracting their total span from the total available span. For very
    // small values of `maxRichResults`, their total span may be larger than
    // `state.availableResultSpan`.
    let globalSuggestedIndexAvailableSpan = Math.min(
      state.availableResultSpan,
      state.globalSuggestedIndexResultSpan
    );
    state.availableResultSpan -= globalSuggestedIndexAvailableSpan;

    if (state.maxHeuristicResultSpan) {
      if (lazy.UrlbarPrefs.get("experimental.hideHeuristic")) {
        // The heuristic is hidden. The muxer will include it but the view will
        // hide it. Increase the available span to compensate so that the total
        // visible span accurately reflects `context.maxResults`.
        state.availableResultSpan += state.maxHeuristicResultSpan;
      } else if (context.maxResults > 0) {
        // `context.maxResults` is positive. Make sure there's room for the
        // heuristic even if it means exceeding `context.maxResults`.
        state.availableResultSpan = Math.max(
          state.availableResultSpan,
          state.maxHeuristicResultSpan
        );
      }
    }

    // Show Top Sites above trending results.
    let showSearchSuggestionsFirst = !(
      lazy.UrlbarPrefs.get("suggest.trending") && !context.searchString
    );
    // Determine the result groups to use for this sort.  In search mode with
    // an engine, show search suggestions first.
    let rootGroup =
      context.searchMode?.engineName || !showSearchSuggestionsFirst
        ? lazy.UrlbarPrefs.makeResultGroups({ showSearchSuggestionsFirst })
        : lazy.UrlbarPrefs.resultGroups;
    lazy.logger.debug(`Groups: ${JSON.stringify(rootGroup)}`);

    // Fill the root group.
    let [sortedResults] = this._fillGroup(
      rootGroup,
      { availableSpan: state.availableResultSpan, maxResultCount: Infinity },
      state
    );

    // Add global suggestedIndex results.
    let globalSuggestedIndexResults = state.resultsByGroup.get(
      UrlbarUtils.RESULT_GROUP.SUGGESTED_INDEX
    );
    if (globalSuggestedIndexResults) {
      this._addSuggestedIndexResults(
        globalSuggestedIndexResults,
        sortedResults,
        {
          availableSpan: globalSuggestedIndexAvailableSpan,
          maxResultCount: Infinity,
        },
        state
      );
    }

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
      suggestedIndexResultsByGroup: new Map(),
      strippedUrlToTopPrefixAndTitle: new Map(
        state.strippedUrlToTopPrefixAndTitle
      ),
      urlToTabResultType: new Map(state.urlToTabResultType),
      addedRemoteTabUrls: new Set(state.addedRemoteTabUrls),
      addedSwitchTabUrls: new Set(state.addedSwitchTabUrls),
      suggestions: new Set(state.suggestions),
      addedResultUrls: new Set(state.addedResultUrls),
    });

    // Deep copy the `resultsByGroup` maps.
    for (let key of ["resultsByGroup", "suggestedIndexResultsByGroup"]) {
      for (let [group, results] of state[key]) {
        copy[key].set(group, [...results]);
      }
    }

    return copy;
  }

  /**
   * Recursively fills a result group and its children.
   *
   * There are two ways to limit the number of results in a group:
   *
   * (1) By max total result span using the `availableSpan` property. The group
   * will be filled so that the total span of its results does not exceed this
   * value.
   *
   * (2) By max total result count using the `maxResultCount` property. The
   * group will be filled so that the total number of its results does not
   * exceed this value.
   *
   * Both `availableSpan` and `maxResultCount` may be defined, and the group's
   * results will be capped to whichever limit is reached first. If either is
   * not defined, then the group inherits that limit from its parent group.
   *
   * In addition to limiting their total number of results, groups can also
   * control the composition of their child groups by using flex ratios. A group
   * can define a `flexChildren: true` property, and in that case each of its
   * children should have a `flex` property. Each child will be filled according
   * to the ratio of its flex value and the sum of the flex values of all the
   * children, similar to HTML flexbox. If some children do not fill up but
   * others do, the filled-up children will be allowed to grow to use up the
   * unfilled space.
   *
   * @param {object} group
   *   The result group to fill.
   * @param {object} limits
   *   An object with optional `availableSpan` and `maxResultCount` properties
   *   as described above. They will be used as the limits for the group.
   * @param {object} state
   *   The muxer state.
   * @returns {Array}
   *   `[results, usedLimits, hasMoreResults]` -- see `_addResults`.
   */
  _fillGroup(group, limits, state) {
    // Get the group's suggestedIndex results. Reminder: `group.group` is a
    // `RESULT_GROUP` constant.
    let suggestedIndexResults;
    let suggestedIndexAvailableSpan = 0;
    let suggestedIndexAvailableCount = 0;
    if ("group" in group) {
      suggestedIndexResults = state.suggestedIndexResultsByGroup.get(
        group.group
      );
      if (suggestedIndexResults) {
        // Subtract them from the group's limits so there will be room for them
        // later. Create a new `limits` object so we don't modify the caller's.
        let [span, resultCount] = suggestedIndexResults.reduce(
          ([sum, count], result) => {
            const spanSize = UrlbarUtils.getSpanForResult(result);
            sum += spanSize;
            if (spanSize) {
              count++;
            }
            return [sum, count];
          },
          [0, 0]
        );
        suggestedIndexAvailableSpan = Math.min(limits.availableSpan, span);
        suggestedIndexAvailableCount = Math.min(
          limits.maxResultCount,
          resultCount
        );
        limits = { ...limits };
        limits.availableSpan -= suggestedIndexAvailableSpan;
        limits.maxResultCount -= suggestedIndexAvailableCount;
      }
    }

    // Fill the group. If it has children, fill them recursively. Otherwise fill
    // the group directly.
    let [results, usedLimits, hasMoreResults] = group.children
      ? this._fillGroupChildren(group, limits, state)
      : this._addResults(group.group, limits, state);

    // Add the group's suggestedIndex results.
    if (suggestedIndexResults) {
      let suggestedIndexUsedLimits = this._addSuggestedIndexResults(
        suggestedIndexResults,
        results,
        {
          availableSpan: suggestedIndexAvailableSpan,
          maxResultCount: suggestedIndexAvailableCount,
        },
        state
      );
      for (let [key, value] of Object.entries(suggestedIndexUsedLimits)) {
        usedLimits[key] += value;
      }
    }

    return [results, usedLimits, hasMoreResults];
  }

  /**
   * Helper for `_fillGroup` that fills a group's children.
   *
   * @param {object} group
   *   The result group to fill. It's assumed to have a `children` property.
   * @param {object} limits
   *   An object with optional `availableSpan` and `maxResultCount` properties
   *   as described in `_fillGroup`.
   * @param {object} state
   *   The muxer state.
   * @param {Array} flexDataArray
   *   See `_updateFlexData`.
   * @returns {Array}
   *   `[results, usedLimits, hasMoreResults]` -- see `_addResults`.
   */
  _fillGroupChildren(group, limits, state, flexDataArray = null) {
    // If the group has flexed children, update the data we use during flex
    // calculations.
    //
    // Handling flex is complicated so we discuss it briefly. We may do multiple
    // passes for a group with flexed children in order to try to optimally fill
    // them. If after one pass some children do not fill up but others do, we'll
    // do another pass that tries to overfill the filled-up children while still
    // respecting their flex ratios. We'll continue to do passes until all
    // children stop filling up or we reach the parent's limits. The way we
    // overfill children is by increasing their individual limits to make up for
    // the unused space in their underfilled siblings. Before starting a new
    // pass, we discard the results from the current pass so the new pass starts
    // with a clean slate. That means we need to copy the global sort state
    // (`state`) before modifying it in the current pass so we can use its
    // original value in the next pass [1].
    //
    // [1] Instead of starting each pass with a clean slate in this way, we
    // could accumulate results with each pass since we only ever add results to
    // flexed children and never remove them. However, that would subvert muxer
    // logic related to the global state (deduping, `_canAddResult`) since we
    // generally assume the muxer adds results in the order they appear.
    let stateCopy;
    if (group.flexChildren) {
      stateCopy = this._copyState(state);
      flexDataArray = this._updateFlexData(group, limits, flexDataArray);
    }

    // Fill each child group, collecting all results in the `results` array.
    let results = [];
    let usedLimits = {};
    for (let key of Object.keys(limits)) {
      usedLimits[key] = 0;
    }
    let anyChildUnderfilled = false;
    let anyChildHasMoreResults = false;
    for (let i = 0; i < group.children.length; i++) {
      let child = group.children[i];
      let flexData = flexDataArray?.[i];

      // Compute the child's limits.
      let childLimits = {};
      for (let key of Object.keys(limits)) {
        childLimits[key] = flexData
          ? flexData.limits[key]
          : Math.min(
              typeof child[key] == "number" ? child[key] : Infinity,
              limits[key] - usedLimits[key]
            );
      }

      // Recurse and fill the child.
      let [childResults, childUsedLimits, childHasMoreResults] =
        this._fillGroup(child, childLimits, state);
      results = results.concat(childResults);
      for (let key of Object.keys(usedLimits)) {
        usedLimits[key] += childUsedLimits[key];
      }
      anyChildHasMoreResults = anyChildHasMoreResults || childHasMoreResults;

      if (flexData?.hasMoreResults) {
        // The child is flexed and we possibly added more results to it.
        flexData.usedLimits = childUsedLimits;
        flexData.hasMoreResults = childHasMoreResults;
        anyChildUnderfilled =
          anyChildUnderfilled ||
          (!childHasMoreResults &&
            [...Object.entries(childLimits)].every(
              ([key, limit]) => flexData.usedLimits[key] < limit
            ));
      }
    }

    // If the children are flexed and some underfilled but others still have
    // more results, do another pass.
    if (anyChildUnderfilled && anyChildHasMoreResults) {
      [results, usedLimits, anyChildHasMoreResults] = this._fillGroupChildren(
        group,
        limits,
        stateCopy,
        flexDataArray
      );

      // Update `state` in place so that it's also updated in the caller.
      for (let [key, value] of Object.entries(stateCopy)) {
        state[key] = value;
      }
    }

    return [results, usedLimits, anyChildHasMoreResults];
  }

  /**
   * Updates flex-related state used while filling a group.
   *
   * @param {object} group
   *   The result group being filled.
   * @param {object} limits
   *   An object defining the group's limits as described in `_fillGroup`.
   * @param {Array} flexDataArray
   *   An array parallel to `group.children`. The object at index i corresponds
   *   to the child in `group.children` at index i. Each object maintains some
   *   flex-related state for its child and is updated during each pass in
   *   `_fillGroup` for `group`. When this method is called in the first pass,
   *   this argument should be null, and the method will create and return a new
   *   `flexDataArray` array that should be used in the remainder of the first
   *   pass and all subsequent passes.
   * @returns {Array}
   *   A new `flexDataArray` when called in the first pass, and `flexDataArray`
   *   itself when called in subsequent passes.
   */
  _updateFlexData(group, limits, flexDataArray) {
    flexDataArray =
      flexDataArray ||
      group.children.map((child, index) => {
        let data = {
          // The index of the corresponding child in `group.children`.
          index,
          // The child's limits.
          limits: {},
          // The fractional parts of the child's unrounded limits; see below.
          limitFractions: {},
          // The used-up portions of the child's limits.
          usedLimits: {},
          // True if `state.resultsByGroup` has more results of the child's
          // `RESULT_GROUP`. This is not related to the child's limits.
          hasMoreResults: true,
          // The child's flex value.
          flex: typeof child.flex == "number" ? child.flex : 0,
        };
        for (let key of Object.keys(limits)) {
          data.limits[key] = 0;
          data.limitFractions[key] = 0;
          data.usedLimits[key] = 0;
        }
        return data;
      });

    // The data objects for children with more results (i.e., that are still
    // fillable).
    let fillableDataArray = [];

    // The sum of the flex values of children with more results.
    let fillableFlexSum = 0;

    for (let data of flexDataArray) {
      if (data.hasMoreResults) {
        fillableFlexSum += data.flex;
        fillableDataArray.push(data);
      }
    }

    // Update each limit.
    for (let [key, limit] of Object.entries(limits)) {
      // Calculate the group's limit only including children with more results.
      let fillableLimit = limit;
      for (let data of flexDataArray) {
        if (!data.hasMoreResults) {
          fillableLimit -= data.usedLimits[key];
        }
      }

      // Allow for the possibility that some children may have gone over limit.
      // `fillableLimit` will be negative in that case.
      fillableLimit = Math.max(fillableLimit, 0);

      // Next we'll compute the limits of children with more results. This value
      // is the sum of those limits. It may differ from `fillableLimit` due to
      // the fact that each individual child limit must be an integer.
      let summedFillableLimit = 0;

      // Compute the limits of children with more results. If there are also
      // children that don't have more results, then these new limits will be
      // larger than they were in the previous pass.
      for (let data of fillableDataArray) {
        let unroundedLimit = fillableLimit * (data.flex / fillableFlexSum);
        // `limitFraction` is the fractional part of the unrounded ideal limit.
        // e.g., for 5.234 it will be 0.234. We use this to minimize the
        // mathematical error when tweaking limits below.
        data.limitFractions[key] = unroundedLimit - Math.floor(unroundedLimit);
        data.limits[key] = Math.round(unroundedLimit);
        summedFillableLimit += data.limits[key];
      }

      // As mentioned above, the sum of the individual child limits may not
      // equal the group's fillable limit. If the sum is smaller, the group will
      // end up with too few results. If it's larger, the group will have the
      // correct number of results (since we stop adding results once limits are
      // reached) but it may end up with a composition that does not reflect the
      // child flex ratios as accurately as possible.
      //
      // In either case, tweak the individual limits so that (1) their sum
      // equals the group's fillable limit, and (2) the composition respects the
      // flex ratios with as little mathematical error as possible.
      if (summedFillableLimit != fillableLimit) {
        // Collect the flex datas with a non-zero limit fractions. We'll round
        // them up or down depending on whether the sum is larger or smaller
        // than the group's fillable limit.
        let fractionalDataArray = fillableDataArray.filter(
          data => data.limitFractions[key]
        );

        let diff;
        if (summedFillableLimit < fillableLimit) {
          // The sum is smaller. We'll increment individual limits until the sum
          // is equal, starting with the child whose limit fraction is closest
          // to 1 in order to minimize error.
          diff = 1;
          fractionalDataArray.sort((a, b) => {
            // Sort by fraction descending so larger fractions are first.
            let cmp = b.limitFractions[key] - a.limitFractions[key];
            // Secondarily sort by index ascending so that children with the
            // same fraction are incremented in the order they appear, allowing
            // earlier children to have larger spans.
            return cmp || a.index - b.index;
          });
        } else if (fillableLimit < summedFillableLimit) {
          // The sum is larger. We'll decrement individual limits until the sum
          // is equal, starting with the child whose limit fraction is closest
          // to 0 in order to minimize error.
          diff = -1;
          fractionalDataArray.sort((a, b) => {
            // Sort by fraction ascending so smaller fractions are first.
            let cmp = a.limitFractions[key] - b.limitFractions[key];
            // Secondarily sort by index descending so that children with the
            // same fraction are decremented in reverse order, allowing earlier
            // children to retain larger spans.
            return cmp || b.index - a.index;
          });
        }

        // Now increment or decrement individual limits until their sum is equal
        // to the group's fillable limit.
        while (summedFillableLimit != fillableLimit) {
          if (!fractionalDataArray.length) {
            // This shouldn't happen, but don't let it break us.
            lazy.logger.error("fractionalDataArray is empty!");
            break;
          }
          let data = flexDataArray[fractionalDataArray.shift().index];
          data.limits[key] += diff;
          summedFillableLimit += diff;
        }
      }
    }

    return flexDataArray;
  }

  /**
   * Adds results to a group using the results from its `RESULT_GROUP` in
   * `state.resultsByGroup`.
   *
   * @param {UrlbarUtils.RESULT_GROUP} groupConst
   *   The group's `RESULT_GROUP`.
   * @param {object} limits
   *   An object defining the group's limits as described in `_fillGroup`.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   * @returns {Array}
   *   `[results, usedLimits, hasMoreResults]` where:
   *     results: A flat array of results in the group, empty if no results
   *       were added.
   *     usedLimits: An object defining the amount of each limit that the
   *       results use. For each possible limit property (see `_fillGroup`),
   *       there will be a corresponding property in this object. For example,
   *       if 3 results are added with a total span of 4, then this object will
   *       be: { maxResultCount: 3, availableSpan: 4 }
   *     hasMoreResults: True if `state.resultsByGroup` has more results of
   *       the same `RESULT_GROUP`. This is not related to the group's limits.
   */
  _addResults(groupConst, limits, state) {
    let usedLimits = {};
    for (let key of Object.keys(limits)) {
      usedLimits[key] = 0;
    }

    // For form history, maxHistoricalSearchSuggestions == 0 implies the user
    // has opted out of form history completely, so we override the max result
    // count here in that case. Other values of maxHistoricalSearchSuggestions
    // are ignored and we use the flex defined on the form history group.
    if (
      groupConst == UrlbarUtils.RESULT_GROUP.FORM_HISTORY &&
      !lazy.UrlbarPrefs.get("maxHistoricalSearchSuggestions")
    ) {
      // Create a new `limits` object so we don't modify the caller's.
      limits = { ...limits };
      limits.maxResultCount = 0;
    }

    let addedResults = [];
    let groupResults = state.resultsByGroup.get(groupConst);
    while (
      groupResults?.length &&
      state.usedResultSpan < state.availableResultSpan &&
      [...Object.entries(limits)].every(([k, limit]) => usedLimits[k] < limit)
    ) {
      let result = groupResults[0];
      if (this._canAddResult(result, state)) {
        if (!this.#updateUsedLimits(result, limits, usedLimits, state)) {
          // Adding the result would exceed the group's available span, so stop
          // adding results to it. Skip the shift() below so the result can be
          // added to later groups.
          break;
        }
        addedResults.push(result);
      }

      // We either add or discard results in the order they appear in
      // `groupResults`, so shift() them off. That way later groups with the
      // same `RESULT_GROUP` won't include results that earlier groups have
      // added or discarded.
      groupResults.shift();
    }

    return [addedResults, usedLimits, !!groupResults?.length];
  }

  /**
   * Returns whether a result can be added to its group given the current sort
   * state.
   *
   * @param {UrlbarResult} result
   *   The result.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   * @returns {boolean}
   *   True if the result can be added and false if it should be discarded.
   */
  // TODO (Bug 1741273): Refactor this method to avoid an eslint complexity
  // error or increase the complexity threshold.
  // eslint-disable-next-line complexity
  _canAddResult(result, state) {
    // QuickSuggest results are shown unless a weather result is also present
    // or they are navigational suggestions that duplicate the heuristic.
    if (result.providerName == lazy.UrlbarProviderQuickSuggest.name) {
      if (state.weatherResult) {
        return false;
      }

      let heuristicUrl = state.context.heuristicResult?.payload.url;
      if (
        heuristicUrl &&
        result.payload.telemetryType == "top_picks" &&
        !lazy.UrlbarPrefs.get("experimental.hideHeuristic")
      ) {
        let opts = {
          stripHttp: true,
          stripHttps: true,
          stripWww: true,
          trimSlash: true,
        };
        result.payload.dupedHeuristic =
          UrlbarUtils.stripPrefixAndTrim(heuristicUrl, opts)[0] ==
          UrlbarUtils.stripPrefixAndTrim(result.payload.url, opts)[0];
        return !result.payload.dupedHeuristic;
      }
      return true;
    }

    // We expect UrlbarProviderPlaces sent us the highest-ranked www. and non-www
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
      state.context.heuristicResult.autofill &&
      !result.autofill &&
      state.context.heuristicResult.payload?.url == result.payload.url &&
      state.context.heuristicResult.type == result.type &&
      !lazy.UrlbarPrefs.get("experimental.hideHeuristic")
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

    if (result.providerName == lazy.UrlbarProviderTabToSearch.name) {
      // Discard the result if a tab-to-search result was added already.
      if (!state.canAddTabToSearch) {
        return false;
      }

      // In cases where the heuristic result is not a URL and we have a
      // tab-to-search result, the tab-to-search provider determined that the
      // typed string is similar to an engine domain. We can let the
      // tab-to-search result through.
      if (state.context.heuristicResult?.type == UrlbarUtils.RESULT_TYPE.URL) {
        // Discard the result if the heuristic result is not autofill and we are
        // not making an exception for a fuzzy match.
        if (
          !state.context.heuristicResult.autofill &&
          !result.payload.satisfiesAutofillThreshold
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
    // suggestions or the heuristic. We do not deduplicate rich suggestions so
    // they do not visually disapear as the suggestion is completed and
    // becomes the same url as the heuristic result.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.payload.lowerCaseSuggestion &&
      !result.isRichSuggestion
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
      !result.isRichSuggestion &&
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

    // Discard switch-to-tab results that dupes another switch-to-tab result.
    if (
      result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH &&
      state.addedSwitchTabUrls.has(makeMapKeyForTabResult(result))
    ) {
      return false;
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
            lazy.UrlbarSearchUtils.serpsAreEquivalent(
              result.payload.url,
              newSerpURL
            )
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
        let searchModeRootDomain =
          lazy.UrlbarSearchUtils.getRootDomainFromEngine(engine);
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

    // Discard history results that dupe the quick suggest result.
    if (
      state.quickSuggestResult &&
      !result.heuristic &&
      result.type == UrlbarUtils.RESULT_TYPE.URL &&
      lazy.QuickSuggest.isURLEquivalentToResultURL(
        result.payload.url,
        state.quickSuggestResult
      )
    ) {
      return false;
    }

    // Discard history results whose URLs were originally sponsored. We use the
    // presence of a partner's URL search param to detect these. The param is
    // defined in the pref below, which is also used for the newtab page.
    if (
      result.source == UrlbarUtils.RESULT_SOURCE.HISTORY &&
      result.type == UrlbarUtils.RESULT_TYPE.URL
    ) {
      let param = Services.prefs.getCharPref(
        "browser.newtabpage.activity-stream.hideTopSitesWithSearchParam"
      );
      if (param) {
        let [key, value] = param.split("=");
        let searchParams;
        try {
          ({ searchParams } = new URL(result.payload.url));
        } catch (error) {}
        if (
          (value === undefined && searchParams?.has(key)) ||
          (value !== undefined && searchParams?.getAll(key).includes(value))
        ) {
          return false;
        }
      }
    }

    // Heuristic results must always be the first result.  If this result is a
    // heuristic but we've already added results, discard it.  Normally this
    // should never happen because the standard result groups are set up so
    // that there's always at most one heuristic and it's always first, but
    // since result groups are stored in a pref and can therefore be modified
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

    // Discard results that have an embedded "url" param with the same value
    // as another result's url
    if (result.payload.url) {
      let urlParams = result.payload.url.split("?").pop();
      let embeddedUrl = new URLSearchParams(urlParams).get("url");

      if (state.addedResultUrls.has(embeddedUrl)) {
        return false;
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
    // check if this result should trigger an exposure
    // if so mark the result properties and skip the rest of the state setting.
    if (this._checkAndSetExposureProperties(result)) {
      return;
    }

    // Keep track of the largest heuristic result span.
    if (result.heuristic && this._canAddResult(result, state)) {
      state.maxHeuristicResultSpan = Math.max(
        state.maxHeuristicResultSpan,
        UrlbarUtils.getSpanForResult(result)
      );
    }

    // Keep track of the total span of global suggestedIndex results so we can
    // make room for them at the end of the sort. Tab-to-search results are an
    // exception: There can be multiple TTS results but only one will be shown,
    // so we track the max TTS span separately.
    if (
      result.hasSuggestedIndex &&
      !result.isSuggestedIndexRelativeToGroup &&
      this._canAddResult(result, state)
    ) {
      let span = UrlbarUtils.getSpanForResult(result);
      if (result.providerName == lazy.UrlbarProviderTabToSearch.name) {
        state.maxTabToSearchResultSpan = Math.max(
          state.maxTabToSearchResultSpan,
          span
        );
      } else {
        state.globalSuggestedIndexResultSpan += span;
      }
    }

    // Save some state we'll use later to dedupe URL results.
    if (
      (result.type == UrlbarUtils.RESULT_TYPE.URL ||
        result.type == UrlbarUtils.RESULT_TYPE.KEYWORD) &&
      result.payload.url &&
      (!result.heuristic || !lazy.UrlbarPrefs.get("experimental.hideHeuristic"))
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
      let prefixRank = UrlbarUtils.getPrefixRank(prefix);
      let topPrefixData = state.strippedUrlToTopPrefixAndTitle.get(strippedUrl);
      let topPrefixRank = topPrefixData ? topPrefixData.rank : -1;
      if (
        topPrefixRank < prefixRank ||
        // If a quick suggest result has the same stripped URL and prefix rank
        // as another result, store the quick suggest as the top rank so we
        // discard the other during deduping. That happens after the user picks
        // the quick suggest: The URL is added to history and later both a
        // history result and the quick suggest may match a query.
        (topPrefixRank == prefixRank &&
          result.providerName == lazy.UrlbarProviderQuickSuggest.name)
      ) {
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
          !state.urlToTabResultType.has(makeMapKeyForTabResult(result))))
    ) {
      // url => result type
      state.urlToTabResultType.set(makeMapKeyForTabResult(result), result.type);
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

    if (result.providerName == lazy.UrlbarProviderQuickSuggest.name) {
      state.quickSuggestResult = result;
    }

    if (result.providerName == lazy.UrlbarProviderWeather.name) {
      state.weatherResult = result;
    }

    state.hasUnitConversionResult =
      state.hasUnitConversionResult || result.providerName == "UnitConversion";

    // Keep track of result urls to dedupe results with the same url embedded
    // in its query string
    if (result.payload.url) {
      state.addedResultUrls.add(result.payload.url);
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
    // bail early if the result will be hidden from the final view.
    if (result.exposureResultHidden) {
      return;
    }

    // Update heuristic state.
    if (result.heuristic) {
      state.context.heuristicResult = result;
      if (
        result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.payload.query &&
        !lazy.UrlbarPrefs.get("experimental.hideHeuristic")
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
      !Services.search.separatePrivateDefaultUrlbarResultEnabled ||
      (state.canShowPrivateSearch &&
        (result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
          result.payload.providesSearchMode ||
          (result.heuristic && result.payload.keyword)))
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
    if (result.providerName == lazy.UrlbarProviderTabToSearch.name) {
      state.canAddTabToSearch = false;
      // We want to record in urlbar.tips once per engagement per engine. Since
      // whether these results are shown is dependent on the Muxer, we must
      // add to `enginesShown` here.
      if (result.payload.dynamicType) {
        lazy.UrlbarProviderTabToSearch.enginesShown.onboarding.add(
          result.payload.engine
        );
      } else {
        lazy.UrlbarProviderTabToSearch.enginesShown.regular.add(
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

    // Keep track of which switch tabs we've added to dedupe switch tabs.
    if (result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH) {
      state.addedSwitchTabUrls.add(makeMapKeyForTabResult(result));
    }
  }

  /**
   * Inserts results with suggested indexes. This can be called for either
   * global or group-relative suggestedIndex results. It should be called after
   * `sortedResults` has been filled in.
   *
   * @param {Array} suggestedIndexResults
   *   Results with a `suggestedIndex` property.
   * @param {Array} sortedResults
   *   The sorted results. For global suggestedIndex results, this should be the
   *   final list of all results before suggestedIndex results are inserted. For
   *   group-relative suggestedIndex results, this should be the final list of
   *   results in the group before group-relative suggestedIndex results are
   *   inserted.
   * @param {object} limits
   *   An object defining span and count limits. See `_fillGroup()`.
   * @param {object} state
   *   Global state that we use to make decisions during this sort.
   * @returns {object}
   *   A `usedLimits` object that describes the total span and count of all the
   *   added results. See `_addResults`.
   */
  _addSuggestedIndexResults(
    suggestedIndexResults,
    sortedResults,
    limits,
    state
  ) {
    let usedLimits = {
      availableSpan: 0,
      maxResultCount: 0,
    };

    if (!suggestedIndexResults?.length) {
      // This is just a slight optimization; no need to continue.
      return usedLimits;
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
    positive.sort((a, b) => {
      if (a.suggestedIndex !== b.suggestedIndex) {
        return a.suggestedIndex - b.suggestedIndex;
      }

      if (a.providerName === b.providerName) {
        return 0;
      }

      // If same suggestedIndex, change the displaying order along to following
      // provider priority.
      // TabToSearch > QuickSuggest > Other providers
      if (a.providerName === lazy.UrlbarProviderTabToSearch.name) {
        return 1;
      }
      if (b.providerName === lazy.UrlbarProviderTabToSearch.name) {
        return -1;
      }
      if (a.providerName === lazy.UrlbarProviderQuickSuggest.name) {
        return 1;
      }
      if (b.providerName === lazy.UrlbarProviderQuickSuggest.name) {
        return -1;
      }

      return 0;
    });

    // Conversely, sort the negative results descending so that results at the
    // front of the array don't end up offset by later insertions at the end.
    negative.sort((a, b) => b.suggestedIndex - a.suggestedIndex);

    // Insert the results. We start with the positive results because we have
    // tests that assume they're inserted first. In practice it shouldn't matter
    // because there's no good reason we would ever have a negative result come
    // before a positive result in the same query. Even if we did, we have to
    // insert one before the other, and there's no right or wrong order.
    for (let results of [positive, negative]) {
      let prevResult;
      let prevIndex;
      for (let result of results) {
        if (this._canAddResult(result, state)) {
          if (!this.#updateUsedLimits(result, limits, usedLimits, state)) {
            return usedLimits;
          }

          let index;
          if (
            prevResult &&
            prevResult.suggestedIndex == result.suggestedIndex
          ) {
            index = prevIndex;
          } else {
            index =
              result.suggestedIndex >= 0
                ? Math.min(result.suggestedIndex, sortedResults.length)
                : Math.max(result.suggestedIndex + sortedResults.length + 1, 0);
          }
          prevResult = result;
          prevIndex = index;
          sortedResults.splice(index, 0, result);
        }
      }
    }

    return usedLimits;
  }

  /**
   * Checks whether adding a result would exceed the given limits. If the limits
   * would be exceeded, this returns false and does nothing else. If the limits
   * would not be exceeded, the given used limits and state are updated to
   * account for the result, true is returned, and the caller should then add
   * the result to its list of sorted results.
   *
   * @param {UrlbarResult} result
   *   The result.
   * @param {object} limits
   *   An object defining span and count limits. See `_fillGroup()`.
   * @param {object} usedLimits
   *   An object with parallel properties to `limits` that describes how much of
   *   the limits have been used. See `_addResults()`.
   * @param {object} state
   *   The muxer state.
   * @returns {boolean}
   *   True if the limits were updated and the result can be added and false
   *   otherwise.
   */
  #updateUsedLimits(result, limits, usedLimits, state) {
    let span = UrlbarUtils.getSpanForResult(result);
    let newUsedSpan = usedLimits.availableSpan + span;
    if (limits.availableSpan < newUsedSpan) {
      // Adding the result would exceed the available span.
      return false;
    }

    usedLimits.availableSpan = newUsedSpan;
    if (span) {
      usedLimits.maxResultCount++;
    }

    state.usedResultSpan += span;
    this._updateStatePostAdd(result, state);

    return true;
  }

  /**
   * Checks exposure eligibility and visibility for the given result.
   * If the result passes the exposure check, we set two properties
   * on the UrlbarResult: `result.exposureResultType` a string containing
   * the results of `UrlbarUtils.searchEngagementTelemetryType` and
   * `result.exposureResultHidden` a boolean which indicates whether the
   * result should be hidden from the view.
   *
   *
   * @param {UrlbarResult} result
   *   The result.
   * @returns {boolean}
   *   A boolean indicating if this is a hidden exposure result.
   */
  _checkAndSetExposureProperties(result) {
    const exposureResultsPref = lazy.UrlbarPrefs.get("exposureResults");
    const exposureResults = exposureResultsPref?.split(",");
    if (exposureResults) {
      const telemetryType = UrlbarUtils.searchEngagementTelemetryType(result);
      if (exposureResults.includes(telemetryType)) {
        result.exposureResultType = telemetryType;
        result.exposureResultHidden = !lazy.UrlbarPrefs.get(
          "showExposureResults"
        );
      }
    }

    return result.exposureResultHidden;
  }
}

export var UrlbarMuxerUnifiedComplete = new MuxerUnifiedComplete();
