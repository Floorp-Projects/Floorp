/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.sys.mjs",
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

const TELEMETRY_SCALARS = {
  BLOCK_SPONSORED: "contextual.services.quicksuggest.block_sponsored",
  BLOCK_SPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.block_sponsored_bestmatch",
  BLOCK_NONSPONSORED: "contextual.services.quicksuggest.block_nonsponsored",
  BLOCK_NONSPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.block_nonsponsored_bestmatch",
  CLICK: "contextual.services.quicksuggest.click",
  CLICK_NONSPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.click_nonsponsored_bestmatch",
  CLICK_SPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.click_sponsored_bestmatch",
  HELP: "contextual.services.quicksuggest.help",
  HELP_NONSPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.help_nonsponsored_bestmatch",
  HELP_SPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.help_sponsored_bestmatch",
  IMPRESSION: "contextual.services.quicksuggest.impression",
  IMPRESSION_NONSPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.impression_nonsponsored_bestmatch",
  IMPRESSION_SPONSORED_BEST_MATCH:
    "contextual.services.quicksuggest.impression_sponsored_bestmatch",
};

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderQuickSuggest extends UrlbarProvider {
  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "UrlbarProviderQuickSuggest";
  }

  /**
   * The type of the provider.
   *
   * @returns {UrlbarUtils.PROVIDER_TYPE}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.NETWORK;
  }

  /**
   * @returns {object} An object mapping from mnemonics to scalar names.
   */
  get TELEMETRY_SCALARS() {
    return { ...TELEMETRY_SCALARS };
  }

  getPriority(context) {
    if (!context.searchString) {
      // Zero-prefix suggestions have the same priority as top sites.
      return lazy.UrlbarProviderTopSites.PRIORITY;
    }
    return super.getPriority(context);
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    this._resultFromLastQuery = null;

    // If the sources don't include search or the user used a restriction
    // character other than search, don't allow any suggestions.
    if (
      !queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.SEARCH) ||
      (queryContext.restrictSource &&
        queryContext.restrictSource != UrlbarUtils.RESULT_SOURCE.SEARCH)
    ) {
      return false;
    }

    if (
      !lazy.UrlbarPrefs.get("quickSuggestEnabled") ||
      queryContext.isPrivate ||
      queryContext.searchMode
    ) {
      return false;
    }

    // Trim only the start of the search string because a trailing space can
    // affect the suggestions.
    this._searchString = queryContext.searchString.trimStart();

    if (!this._searchString) {
      return !!lazy.QuickSuggest.weather.suggestion;
    }
    return (
      lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") ||
      lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") ||
      lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled")
    );
  }

  /**
   * Starts querying. Extended classes should return a Promise resolved when the
   * provider is done searching AND returning results.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @returns {Promise}
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;
    let searchString = this._searchString;

    if (!searchString) {
      let result = this._makeWeatherResult();
      if (result) {
        addCallback(this, result);
        this._resultFromLastQuery = result;
      }
      return;
    }

    // There are two sources for quick suggest: remote settings and Merino.
    let promises = [];
    if (lazy.UrlbarPrefs.get("quickSuggestRemoteSettingsEnabled")) {
      promises.push(lazy.QuickSuggest.remoteSettings.fetch(searchString));
    }
    if (
      lazy.UrlbarPrefs.get("merinoEnabled") &&
      lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled") &&
      queryContext.allowRemoteResults()
    ) {
      promises.push(this._fetchMerinoSuggestions(queryContext, searchString));
    }

    // Wait for both sources to finish before adding a suggestion.
    let allSuggestions = await Promise.all(promises);
    if (instance != this.queryInstance) {
      return;
    }

    // Filter suggestions, keeping in mind both the remote settings and Merino
    // fetches return null when there are no matches. Take the remaining one
    // with the largest score.
    allSuggestions = await Promise.all(
      allSuggestions
        .flat()
        .map(async s => (s && (await this._canAddSuggestion(s)) ? s : null))
    );
    if (instance != this.queryInstance) {
      return;
    }
    const suggestion = allSuggestions
      .filter(Boolean)
      .sort((a, b) => b.score - a.score)[0];

    if (!suggestion) {
      return;
    }

    // Replace the suggestion's template substrings, but first save the original
    // URL before its timestamp template is replaced.
    let originalUrl = suggestion.url;
    lazy.QuickSuggest.replaceSuggestionTemplates(suggestion);

    let payload = {
      originalUrl,
      url: suggestion.url,
      urlTimestampIndex: suggestion.urlTimestampIndex,
      icon: suggestion.icon,
      sponsoredImpressionUrl: suggestion.impression_url,
      sponsoredClickUrl: suggestion.click_url,
      sponsoredBlockId: suggestion.block_id,
      sponsoredAdvertiser: suggestion.advertiser,
      sponsoredIabCategory: suggestion.iab_category,
      isSponsored: suggestion.is_sponsored,
      helpUrl: lazy.QuickSuggest.HELP_URL,
      helpL10n: { id: "firefox-suggest-urlbar-learn-more" },
      source: suggestion.source,
      requestId: suggestion.request_id,
    };

    // Determine if the suggestion itself is a best match.
    let isSuggestionBestMatch = false;
    if (typeof suggestion._test_is_best_match == "boolean") {
      isSuggestionBestMatch = suggestion._test_is_best_match;
    } else if (suggestion?.is_top_pick) {
      isSuggestionBestMatch = true;
    } else if (lazy.QuickSuggest.remoteSettings.config.best_match) {
      let { best_match } = lazy.QuickSuggest.remoteSettings.config;
      isSuggestionBestMatch =
        best_match.min_search_string_length <= searchString.length &&
        !best_match.blocked_suggestion_ids.includes(suggestion.block_id);
    }

    // Determine if the urlbar result should be a best match.
    let isResultBestMatch =
      isSuggestionBestMatch &&
      lazy.UrlbarPrefs.get("bestMatchEnabled") &&
      lazy.UrlbarPrefs.get("suggest.bestmatch");
    if (isResultBestMatch) {
      // Show the result as a best match. Best match titles don't include the
      // `full_keyword`, and the user's search string is highlighted.
      payload.title = [suggestion.title, UrlbarUtils.HIGHLIGHT.TYPED];
      payload.isBlockable = lazy.UrlbarPrefs.get("bestMatchBlockingEnabled");
      payload.blockL10n = { id: "firefox-suggest-urlbar-block" };
    } else {
      // Show the result as a usual quick suggest. Include the `full_keyword`
      // and highlight the parts that aren't in the search string.
      payload.title = suggestion.title;
      payload.isBlockable = lazy.UrlbarPrefs.get("quickSuggestBlockingEnabled");
      payload.blockL10n = { id: "firefox-suggest-urlbar-block" };
      payload.qsSuggestion = [
        suggestion.full_keyword,
        UrlbarUtils.HIGHLIGHT.SUGGESTED,
      ];
    }

    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );

    if (isResultBestMatch) {
      result.isBestMatch = true;
      result.suggestedIndex = 1;
    } else if (
      !isNaN(suggestion.position) &&
      lazy.UrlbarPrefs.get("quickSuggestAllowPositionInSuggestions")
    ) {
      result.suggestedIndex = suggestion.position;
    } else {
      result.isSuggestedIndexRelativeToGroup = true;
      result.suggestedIndex = lazy.UrlbarPrefs.get(
        suggestion.is_sponsored
          ? "quickSuggestSponsoredIndex"
          : "quickSuggestNonSponsoredIndex"
      );
    }

    addCallback(this, result);

    this._resultFromLastQuery = result;

    // The user triggered a suggestion. Depending on the experiment the user is
    // enrolled in (if any), we may need to record the Nimbus exposure event.
    //
    // If the user is in a best match experiment:
    //   Record if the suggestion is itself a best match and either of the
    //   following are true:
    //   * The best match feature is enabled (i.e., the user is in a treatment
    //     branch), and the user has not disabled best match
    //   * The best match feature is disabled (i.e., the user is in the control
    //     branch)
    // Else if the user is not in a modal experiment:
    //   Record the event
    if (
      lazy.UrlbarPrefs.get("isBestMatchExperiment") ||
      lazy.UrlbarPrefs.get("experimentType") === "best-match"
    ) {
      if (
        isSuggestionBestMatch &&
        (!lazy.UrlbarPrefs.get("bestMatchEnabled") ||
          lazy.UrlbarPrefs.get("suggest.bestmatch"))
      ) {
        lazy.QuickSuggest.ensureExposureEventRecorded();
      }
    } else if (lazy.UrlbarPrefs.get("experimentType") !== "modal") {
      lazy.QuickSuggest.ensureExposureEventRecorded();
    }
  }

  /**
   * Called when the result's block button is picked. If the provider can block
   * the result, it should do so and return true. If the provider cannot block
   * the result, it should return false. The meaning of "blocked" depends on the
   * provider and the type of result.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   * @param {UrlbarResult} result
   *   The result that should be blocked.
   * @returns {boolean}
   *   Whether the result was blocked.
   */
  blockResult(queryContext, result) {
    if (result.payload.merinoProvider == "accuweather") {
      this.logger.info("Blocking weather result");
      lazy.UrlbarPrefs.set("suggest.weather", false);
      return true;
    }

    if (
      (!result.isBestMatch &&
        !lazy.UrlbarPrefs.get("quickSuggestBlockingEnabled")) ||
      (result.isBestMatch && !lazy.UrlbarPrefs.get("bestMatchBlockingEnabled"))
    ) {
      this.logger.info("Blocking disabled, ignoring block");
      return false;
    }

    this.logger.info("Blocking result: " + JSON.stringify(result));
    lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
    this._recordEngagementTelemetry(result, queryContext.isPrivate, "block");
    return true;
  }

  /**
   * Called when the user starts and ends an engagement with the urlbar.  For
   * details on parameters, see UrlbarProvider.onEngagement().
   *
   * @param {boolean} isPrivate
   *   True if the engagement is in a private context.
   * @param {string} state
   *   The state of the engagement, one of: start, engagement, abandonment,
   *   discard
   * @param {UrlbarQueryContext} queryContext
   *   The engagement's query context.  This is *not* guaranteed to be defined
   *   when `state` is "start".  It will always be defined for "engagement" and
   *   "abandonment".
   * @param {object} details
   *   This is defined only when `state` is "engagement" or "abandonment", and
   *   it describes the search string and picked result.
   */
  onEngagement(isPrivate, state, queryContext, details) {
    let result = this._resultFromLastQuery;
    this._resultFromLastQuery = null;

    // Reset the Merino session ID when an engagement ends. Per spec, for the
    // user's privacy, we don't keep it around between engagements. It wouldn't
    // hurt to do this on start too, it's just not necessary if we always do it
    // on end.
    if (state != "start") {
      this._merino?.resetSession();
    }

    // Impression and clicked telemetry are both recorded on engagement. We
    // define "impression" to mean a quick suggest result was present in the
    // view when any result was picked.
    if (state == "engagement") {
      // Find the quick suggest result that's currently visible in the view.
      // It's probably the result from the last query so check it first, but due
      // to the async nature of how results are added to the view and made
      // visible, it may not be.
      if (
        result &&
        (result.rowIndex < 0 ||
          queryContext.view?.visibleResults?.[result.rowIndex] != result)
      ) {
        // The result from the last query isn't visible.
        result = null;
      }

      // If the result isn't visible, find a visible one. Quick suggest results
      // typically appear last in the view, so do a reverse search.
      if (!result) {
        result = queryContext.view?.visibleResults?.findLast(
          r => r.providerName == this.name
        );
      }

      // Finally, record telemetry if there's a visible result.
      if (result) {
        this._recordEngagementTelemetry(
          result,
          isPrivate,
          details.selIndex == result.rowIndex ? details.selType : ""
        );
      }
    }
  }

  /**
   * Records engagement telemetry. This should be called only at the end of an
   * engagement when a quick suggest result is present or when a quick suggest
   * result is blocked.
   *
   * @param {UrlbarResult} result
   *   The quick suggest result that was present (and possibly picked) at the
   *   end of the engagement or that was blocked.
   * @param {boolean} isPrivate
   *   Whether the engagement is in a private context.
   * @param {string} selType
   *   This parameter indicates the part of the row the user picked, if any, and
   *   should be one of the following values:
   *
   *   - "": The user didn't pick the row or any part of it
   *   - "quicksuggest": The user picked the main part of the row
   *   - "help": The user picked the help button
   *   - "block": The user picked the block button or used the key shortcut
   *
   *   An empty string means the user picked some other row to end the
   *   engagement, not the quick suggest row. In that case only impression
   *   telemetry will be recorded.
   *
   *   A non-empty string means the user picked the quick suggest row or some
   *   part of it, and both impression and click telemetry will be recorded. The
   *   non-empty-string values come from the `details.selType` passed in to
   *   `onEngagement()`; see `TelemetryEvent.typeFromElement()`.
   */
  _recordEngagementTelemetry(result, isPrivate, selType) {
    // Update impression stats.
    lazy.QuickSuggest.impressionCaps.updateStats(
      result.payload.isSponsored ? "sponsored" : "nonsponsored"
    );

    // Indexes recorded in quick suggest telemetry are 1-based, so add 1 to the
    // 0-based `result.rowIndex`.
    let telemetryResultIndex = result.rowIndex + 1;

    // impression scalars
    Services.telemetry.keyedScalarAdd(
      TELEMETRY_SCALARS.IMPRESSION,
      telemetryResultIndex,
      1
    );
    if (result.isBestMatch) {
      Services.telemetry.keyedScalarAdd(
        result.payload.isSponsored
          ? TELEMETRY_SCALARS.IMPRESSION_SPONSORED_BEST_MATCH
          : TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED_BEST_MATCH,
        telemetryResultIndex,
        1
      );
    }

    // scalars related to clicking the result and other elements in its row
    let clickScalars = [];
    switch (selType) {
      case "quicksuggest":
        clickScalars.push(TELEMETRY_SCALARS.CLICK);
        if (result.isBestMatch) {
          clickScalars.push(
            result.payload.isSponsored
              ? TELEMETRY_SCALARS.CLICK_SPONSORED_BEST_MATCH
              : TELEMETRY_SCALARS.CLICK_NONSPONSORED_BEST_MATCH
          );
        }
        break;
      case "help":
        clickScalars.push(TELEMETRY_SCALARS.HELP);
        if (result.isBestMatch) {
          clickScalars.push(
            result.payload.isSponsored
              ? TELEMETRY_SCALARS.HELP_SPONSORED_BEST_MATCH
              : TELEMETRY_SCALARS.HELP_NONSPONSORED_BEST_MATCH
          );
        }
        break;
      case "block":
        clickScalars.push(
          result.payload.isSponsored
            ? TELEMETRY_SCALARS.BLOCK_SPONSORED
            : TELEMETRY_SCALARS.BLOCK_NONSPONSORED
        );
        if (result.isBestMatch) {
          clickScalars.push(
            result.payload.isSponsored
              ? TELEMETRY_SCALARS.BLOCK_SPONSORED_BEST_MATCH
              : TELEMETRY_SCALARS.BLOCK_NONSPONSORED_BEST_MATCH
          );
        }
        break;
      default:
        if (selType) {
          this.logger.error(
            "Engagement telemetry error, unknown selType: " + selType
          );
        }
        break;
    }
    for (let scalar of clickScalars) {
      Services.telemetry.keyedScalarAdd(scalar, telemetryResultIndex, 1);
    }

    // engagement event
    let match_type = result.isBestMatch ? "best-match" : "firefox-suggest";
    Services.telemetry.recordEvent(
      lazy.QuickSuggest.TELEMETRY_EVENT_CATEGORY,
      "engagement",
      selType == "quicksuggest" ? "click" : selType || "impression_only",
      "",
      {
        match_type,
        position: String(telemetryResultIndex),
        suggestion_type: result.payload.isSponsored
          ? "sponsored"
          : "nonsponsored",
      }
    );

    // custom pings
    if (!isPrivate) {
      // `is_clicked` is whether the user clicked the suggestion. `selType` will
      // be "quicksuggest" in that case. See this method's JSDoc for all
      // possible `selType` values.
      let is_clicked = selType == "quicksuggest";
      let payload = {
        match_type,
        // Always use lowercase to make the reporting consistent
        advertiser: result.payload.sponsoredAdvertiser.toLocaleLowerCase(),
        block_id: result.payload.sponsoredBlockId,
        improve_suggest_experience_checked: lazy.UrlbarPrefs.get(
          "quicksuggest.dataCollection.enabled"
        ),
        position: telemetryResultIndex,
        request_id: result.payload.requestId,
      };

      // impression
      lazy.PartnerLinkAttribution.sendContextualServicesPing(
        {
          ...payload,
          is_clicked,
          reporting_url: result.payload.sponsoredImpressionUrl,
        },
        lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION
      );

      // click
      if (is_clicked) {
        lazy.PartnerLinkAttribution.sendContextualServicesPing(
          {
            ...payload,
            reporting_url: result.payload.sponsoredClickUrl,
          },
          lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION
        );
      }

      // block
      if (selType == "block") {
        lazy.PartnerLinkAttribution.sendContextualServicesPing(
          {
            ...payload,
            iab_category: result.payload.sponsoredIabCategory,
          },
          lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK
        );
      }
    }
  }

  /**
   * Cancels the current query.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   */
  cancelQuery(queryContext) {
    // Cancel the Merino timeout timer so it doesn't fire and record a timeout.
    // If it's already canceled or has fired, this is a no-op.
    this._merino?.cancelTimeoutTimer();

    // Don't abort the Merino fetch if one is ongoing. By design we allow
    // fetches to finish so we can record their latency.
  }

  /**
   * Fetches Merino suggestions.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   * @param {string} searchString
   *   The search string.
   * @returns {Array}
   *   The Merino suggestions or null if there's an error or unexpected
   *   response.
   */
  async _fetchMerinoSuggestions(queryContext, searchString) {
    if (!this._merino) {
      this._merino = new lazy.MerinoClient(this.name);
    }

    let providers;
    if (
      !lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") &&
      !lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") &&
      !lazy.UrlbarPrefs.get("merinoProviders")
    ) {
      // Data collection is enabled but suggestions are not. Use an empty list
      // of providers to tell Merino not to fetch any suggestions.
      providers = [];
    }

    let suggestions = await this._merino.fetch({
      providers,
      query: searchString,
    });

    return suggestions;
  }

  /**
   * Returns a UrlbarResult for the current prefetched weather suggestion if
   * there is one.
   *
   * @returns {UrlbarResult}
   *   A result or null if there's no weather suggestion.
   */
  _makeWeatherResult() {
    let { suggestion } = lazy.QuickSuggest.weather;
    if (!suggestion) {
      return null;
    }

    let unit = Services.locale.appLocaleAsBCP47 == "en-US" ? "f" : "c";
    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
      {
        title:
          suggestion.city_name +
          " • " +
          suggestion.current_conditions.temperature[unit] +
          "° " +
          suggestion.current_conditions.summary +
          " • " +
          suggestion.forecast.summary +
          " • H " +
          suggestion.forecast.high[unit] +
          "° • L " +
          suggestion.forecast.low[unit] +
          "°",
        url: suggestion.url,
        icon: "chrome://global/skin/icons/highlights.svg",
        helpUrl: lazy.QuickSuggest.HELP_URL,
        helpL10n: { id: "firefox-suggest-urlbar-learn-more" },
        isBlockable: true,
        blockL10n: { id: "firefox-suggest-urlbar-block" },
        requestId: suggestion.request_id,
        source: suggestion.source,
        merinoProvider: suggestion.provider,
      }
    );

    result.suggestedIndex = 0;
    return result;
  }

  /**
   * Returns whether a given suggestion can be added for a query, assuming the
   * provider itself should be active.
   *
   * @param {object} suggestion
   *   The suggestion to check.
   * @returns {boolean}
   *   Whether the suggestion can be added.
   */
  async _canAddSuggestion(suggestion) {
    this.logger.info("Checking if suggestion can be added");
    this.logger.debug(JSON.stringify({ suggestion }));

    // Return false if suggestions are disabled.
    if (
      (suggestion.is_sponsored &&
        !lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored")) ||
      (!suggestion.is_sponsored &&
        !lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored"))
    ) {
      this.logger.info("Suggestions disabled, not adding suggestion");
      return false;
    }

    // Return false if an impression cap has been hit.
    if (
      (suggestion.is_sponsored &&
        lazy.UrlbarPrefs.get("quickSuggestImpressionCapsSponsoredEnabled")) ||
      (!suggestion.is_sponsored &&
        lazy.UrlbarPrefs.get("quickSuggestImpressionCapsNonSponsoredEnabled"))
    ) {
      let type = suggestion.is_sponsored ? "sponsored" : "nonsponsored";
      let hitStats = lazy.QuickSuggest.impressionCaps.getHitStats(type);
      if (hitStats) {
        this.logger.info("Impression cap(s) hit, not adding suggestion");
        this.logger.debug(JSON.stringify({ type, hitStats }));
        return false;
      }
    }

    // Return false if the suggestion is blocked.
    if (await lazy.QuickSuggest.blockedSuggestions.has(suggestion.url)) {
      this.logger.info("Suggestion blocked, not adding suggestion");
      return false;
    }

    this.logger.info("Suggestion can be added");
    return true;
  }

  // The result we added during the most recent query.
  _resultFromLastQuery = null;

  // The Merino client.
  _merino = null;
}

export var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
