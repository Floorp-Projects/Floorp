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
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

// The types of quick suggest results supported by this provider.
//
// IMPORTANT: These values are used in telemetry, so be careful about changing
// them. See:
//
//   UrlbarUtils.searchEngagementTelemetryType() (Glean telemetry)
//   UrlbarUtils.telemetryTypeFromResult() (legacy telemetry)
const RESULT_SUBTYPE = {
  DYNAMIC_WIKIPEDIA: "dynamic_wikipedia",
  NAVIGATIONAL: "navigational",
  NONSPONSORED: "suggest_non_sponsor",
  SPONSORED: "suggest_sponsor",
};

const TELEMETRY_PREFIX = "contextual.services.quicksuggest";

const TELEMETRY_SCALARS = {
  BLOCK_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.block_dynamic_wikipedia`,
  BLOCK_NONSPONSORED: `${TELEMETRY_PREFIX}.block_nonsponsored`,
  BLOCK_NONSPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.block_nonsponsored_bestmatch`,
  BLOCK_SPONSORED: `${TELEMETRY_PREFIX}.block_sponsored`,
  BLOCK_SPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.block_sponsored_bestmatch`,
  CLICK_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.click_dynamic_wikipedia`,
  CLICK_NAV_NOTMATCHED: `${TELEMETRY_PREFIX}.click_nav_notmatched`,
  CLICK_NAV_SHOWN_HEURISTIC: `${TELEMETRY_PREFIX}.click_nav_shown_heuristic`,
  CLICK_NAV_SHOWN_NAV: `${TELEMETRY_PREFIX}.click_nav_shown_nav`,
  CLICK_NAV_SUPERCEDED: `${TELEMETRY_PREFIX}.click_nav_superceded`,
  CLICK_NONSPONSORED: `${TELEMETRY_PREFIX}.click_nonsponsored`,
  CLICK_NONSPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.click_nonsponsored_bestmatch`,
  CLICK_SPONSORED: `${TELEMETRY_PREFIX}.click_sponsored`,
  CLICK_SPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.click_sponsored_bestmatch`,
  HELP_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.help_dynamic_wikipedia`,
  HELP_NONSPONSORED: `${TELEMETRY_PREFIX}.help_nonsponsored`,
  HELP_NONSPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.help_nonsponsored_bestmatch`,
  HELP_SPONSORED: `${TELEMETRY_PREFIX}.help_sponsored`,
  HELP_SPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.help_sponsored_bestmatch`,
  IMPRESSION_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.impression_dynamic_wikipedia`,
  IMPRESSION_NAV_NOTMATCHED: `${TELEMETRY_PREFIX}.impression_nav_notmatched`,
  IMPRESSION_NAV_SHOWN: `${TELEMETRY_PREFIX}.impression_nav_shown`,
  IMPRESSION_NAV_SUPERCEDED: `${TELEMETRY_PREFIX}.impression_nav_superceded`,
  IMPRESSION_NONSPONSORED: `${TELEMETRY_PREFIX}.impression_nonsponsored`,
  IMPRESSION_NONSPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.impression_nonsponsored_bestmatch`,
  IMPRESSION_SPONSORED: `${TELEMETRY_PREFIX}.impression_sponsored`,
  IMPRESSION_SPONSORED_BEST_MATCH: `${TELEMETRY_PREFIX}.impression_sponsored_bestmatch`,
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
   * @returns {object}
   *   An object containing the types of quick suggest results supported by this
   *   provider.
   */
  get RESULT_SUBTYPE() {
    return { ...RESULT_SUBTYPE };
  }

  /**
   * @returns {object} An object mapping from mnemonics to scalar names.
   */
  get TELEMETRY_SCALARS() {
    return { ...TELEMETRY_SCALARS };
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
    this.#resultFromLastQuery = null;

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
    let trimmedSearchString = queryContext.searchString.trimStart();
    if (!trimmedSearchString) {
      return false;
    }
    this._trimmedSearchString = trimmedSearchString;

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
    let searchString = this._trimmedSearchString;

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
      helpL10n: {
        id: lazy.UrlbarPrefs.get("resultMenu")
          ? "urlbar-result-menu-learn-more-about-firefox-suggest"
          : "firefox-suggest-urlbar-learn-more",
      },
      blockL10n: {
        id: lazy.UrlbarPrefs.get("resultMenu")
          ? "urlbar-result-menu-dismiss-firefox-suggest"
          : "firefox-suggest-urlbar-block",
      },
      source: suggestion.source,
      requestId: suggestion.request_id,
    };

    // Determine the suggestion subtype.
    if (suggestion.is_top_pick) {
      payload.subtype = RESULT_SUBTYPE.NAVIGATIONAL;
    } else if (suggestion.advertiser == "dynamic-wikipedia") {
      payload.subtype = RESULT_SUBTYPE.DYNAMIC_WIKIPEDIA;
    } else if (suggestion.is_sponsored) {
      payload.subtype = RESULT_SUBTYPE.SPONSORED;
    } else {
      payload.subtype = RESULT_SUBTYPE.NONSPONSORED;
    }

    // Determine if the suggestion itself is a best match.
    let isSuggestionBestMatch = false;
    if (suggestion.is_top_pick) {
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
    } else {
      // Show the result as a usual quick suggest. Include the `full_keyword`
      // and highlight the parts that aren't in the search string.
      payload.title = suggestion.title;
      payload.qsSuggestion = [
        suggestion.full_keyword,
        UrlbarUtils.HIGHLIGHT.SUGGESTED,
      ];
    }
    payload.isBlockable = lazy.UrlbarPrefs.get(
      isResultBestMatch
        ? "bestMatchBlockingEnabled"
        : "quickSuggestBlockingEnabled"
    );

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

    this.#resultFromLastQuery = result;

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
    // Ignore engagements on other results that didn't end the session.
    if (details.result?.providerName != this.name && details.isSessionOngoing) {
      return;
    }

    // Reset the Merino session ID when a session ends. By design for the user's
    // privacy, we don't keep it around between engagements.
    if (state != "start" && !details.isSessionOngoing) {
      this.#merino?.resetSession();
    }

    // Impression and clicked telemetry are both recorded on engagement. We
    // define "impression" to mean a quick suggest result was present in the
    // view when any result was picked.
    if (state == "engagement" && queryContext) {
      // Get the result that's visible in the view. `details.result` is the
      // engaged result, if any; if it's from this provider, then that's the
      // visible result. Otherwise fall back to #getVisibleResultFromLastQuery.
      let { result } = details;
      if (result?.providerName != this.name) {
        result = this.#getVisibleResultFromLastQuery(queryContext.view);
      }

      this.#recordEngagement(queryContext, isPrivate, result, details);
    }

    // Handle dismissals.
    if (
      details.result?.providerName == this.name &&
      details.selType == "dismiss"
    ) {
      this.#dismissResult(queryContext, details.result);
    }

    this.#resultFromLastQuery = null;
  }

  #getVisibleResultFromLastQuery(view) {
    let result = this.#resultFromLastQuery;

    if (
      result?.rowIndex >= 0 &&
      view?.visibleResults?.[result.rowIndex] == result
    ) {
      // The result was visible.
      return result;
    }

    // Find a visible result. Quick suggest results typically appear last in the
    // view, so do a reverse search.
    return view?.visibleResults?.findLast(r => r.providerName == this.name);
  }

  #dismissResult(queryContext, result) {
    if (!result.payload.isBlockable) {
      this.logger.info("Dismissals disabled, ignoring dismissal");
      return;
    }

    this.logger.info("Dismissing result: " + JSON.stringify(result));
    lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
    queryContext.view.controller.removeResult(result);
  }

  /**
   * Records engagement telemetry. This should be called only at the end of an
   * engagement when a quick suggest result is present or when a quick suggest
   * result is dismissed.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   * @param {boolean} isPrivate
   *   Whether the engagement is in a private context.
   * @param {UrlbarResult} result
   *   The quick suggest result that was present (and possibly picked) at the
   *   end of the engagement or that was dismissed. Null if no quick suggest
   *   result was present.
   * @param {object} details
   *   The `details` object that was passed to `onEngagement()`. It must look
   *   like this: `{ selType, selIndex }`
   */
  #recordEngagement(queryContext, isPrivate, result, details) {
    // This is the `selType` of `result` if it was the engaged result. Otherwise
    // it's an empty string.
    let resultSelType = details.result == result ? details.selType : "";

    // Determine if the main part of the row was clicked, as opposed to a button
    // like help or dismiss. When the main part of sponsored and non-sponsored
    // rows is clicked, `selType` will be "quicksuggest". For other rows it will
    // be one of the `RESULT_SUBTYPE` values.
    let resultClicked =
      resultSelType == "quicksuggest" ||
      Object.values(RESULT_SUBTYPE).includes(resultSelType);

    if (result) {
      // Update impression stats.
      lazy.QuickSuggest.impressionCaps.updateStats(
        result.payload.isSponsored ? "sponsored" : "nonsponsored"
      );

      // Record engagement scalars, event, and pings.
      this.#recordEngagementScalars({ result, resultSelType });
      this.#recordEngagementEvent({ result, resultSelType, resultClicked });
      if (!isPrivate) {
        this.#recordEngagementPings({ result, resultSelType, resultClicked });
      }
    }

    // Navigational suggestions telemetry requires special handling and does not
    // depend on a result being visible.
    if (
      lazy.UrlbarPrefs.get("recordNavigationalSuggestionTelemetry") &&
      queryContext.heuristicResult
    ) {
      this.#recordNavSuggestionTelemetry({
        queryContext,
        result,
        resultSelType,
        resultClicked,
        details,
      });
    }
  }

  /**
   * Helper for engagement telemetry that records engagement scalars.
   *
   * @param {object} options
   *   Options object
   * @param {UrlbarResult} options.result
   *   The quick suggest result related to the engagement. Must not be null.
   * @param {string} options.resultSelType
   *   If an element in the result's row was clicked, this should be its
   *   `selType`. Otherwise it should be an empty string.
   */
  #recordEngagementScalars({ result, resultSelType }) {
    // Navigational suggestion scalars are handled separately.
    if (result.payload.subtype == RESULT_SUBTYPE.NAVIGATIONAL) {
      return;
    }

    // Indexes recorded in quick suggest telemetry are 1-based, so add 1 to the
    // 0-based `result.rowIndex`.
    let telemetryResultIndex = result.rowIndex + 1;

    // impression scalars
    let impressionScalars = [];
    switch (result.payload.subtype) {
      case RESULT_SUBTYPE.DYNAMIC_WIKIPEDIA:
        impressionScalars.push(TELEMETRY_SCALARS.IMPRESSION_DYNAMIC_WIKIPEDIA);
        break;
      case RESULT_SUBTYPE.NONSPONSORED:
        impressionScalars.push(TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED);
        if (result.isBestMatch) {
          impressionScalars.push(
            TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED_BEST_MATCH
          );
        }
        break;
      case RESULT_SUBTYPE.SPONSORED:
        impressionScalars.push(TELEMETRY_SCALARS.IMPRESSION_SPONSORED);
        if (result.isBestMatch) {
          impressionScalars.push(
            TELEMETRY_SCALARS.IMPRESSION_SPONSORED_BEST_MATCH
          );
        }
        break;
    }
    for (let scalar of impressionScalars) {
      Services.telemetry.keyedScalarAdd(scalar, telemetryResultIndex, 1);
    }

    // scalars related to clicking the result and other elements in its row
    let clickScalars = [];
    switch (resultSelType) {
      case RESULT_SUBTYPE.DYNAMIC_WIKIPEDIA:
        clickScalars.push(TELEMETRY_SCALARS.CLICK_DYNAMIC_WIKIPEDIA);
        break;
      case "quicksuggest":
        // "quicksuggest" is the `selType` for sponsored and non-sponsored
        // suggestions.
        switch (result.payload.subtype) {
          case RESULT_SUBTYPE.NONSPONSORED:
            clickScalars.push(TELEMETRY_SCALARS.CLICK_NONSPONSORED);
            if (result.isBestMatch) {
              clickScalars.push(
                TELEMETRY_SCALARS.CLICK_NONSPONSORED_BEST_MATCH
              );
            }
            break;
          case RESULT_SUBTYPE.SPONSORED:
            clickScalars.push(TELEMETRY_SCALARS.CLICK_SPONSORED);
            if (result.isBestMatch) {
              clickScalars.push(TELEMETRY_SCALARS.CLICK_SPONSORED_BEST_MATCH);
            }
            break;
        }
        break;
      case "help":
        switch (result.payload.subtype) {
          case RESULT_SUBTYPE.DYNAMIC_WIKIPEDIA:
            clickScalars.push(TELEMETRY_SCALARS.HELP_DYNAMIC_WIKIPEDIA);
            break;
          case RESULT_SUBTYPE.NONSPONSORED:
            clickScalars.push(TELEMETRY_SCALARS.HELP_NONSPONSORED);
            if (result.isBestMatch) {
              clickScalars.push(TELEMETRY_SCALARS.HELP_NONSPONSORED_BEST_MATCH);
            }
            break;
          case RESULT_SUBTYPE.SPONSORED:
            clickScalars.push(TELEMETRY_SCALARS.HELP_SPONSORED);
            if (result.isBestMatch) {
              clickScalars.push(TELEMETRY_SCALARS.HELP_SPONSORED_BEST_MATCH);
            }
            break;
        }
        break;
      case "dismiss":
        switch (result.payload.subtype) {
          case RESULT_SUBTYPE.DYNAMIC_WIKIPEDIA:
            clickScalars.push(TELEMETRY_SCALARS.BLOCK_DYNAMIC_WIKIPEDIA);
            break;
          case RESULT_SUBTYPE.NONSPONSORED:
            clickScalars.push(TELEMETRY_SCALARS.BLOCK_NONSPONSORED);
            if (result.isBestMatch) {
              clickScalars.push(
                TELEMETRY_SCALARS.BLOCK_NONSPONSORED_BEST_MATCH
              );
            }
            break;
          case RESULT_SUBTYPE.SPONSORED:
            clickScalars.push(TELEMETRY_SCALARS.BLOCK_SPONSORED);
            if (result.isBestMatch) {
              clickScalars.push(TELEMETRY_SCALARS.BLOCK_SPONSORED_BEST_MATCH);
            }
            break;
        }
        break;
      default:
        if (resultSelType) {
          this.logger.error(
            "Engagement telemetry error, unknown resultSelType " + resultSelType
          );
        }
        break;
    }
    for (let scalar of clickScalars) {
      Services.telemetry.keyedScalarAdd(scalar, telemetryResultIndex, 1);
    }
  }

  /**
   * Helper for engagement telemetry that records the legacy engagement event.
   *
   * @param {object} options
   *   Options object
   * @param {UrlbarResult} options.result
   *   The quick suggest result related to the engagement. Must not be null.
   * @param {string} options.resultSelType
   *   If an element in the result's row was clicked, this should be its
   *   `selType`. Otherwise it should be an empty string.
   * @param {boolean} options.resultClicked
   *   True if the main part of the result's row was clicked; false if a button
   *   like help or dismiss was clicked or if no part of the row was clicked.
   */
  #recordEngagementEvent({ result, resultSelType, resultClicked }) {
    // Determine the event type we should record:
    //
    // * "click": The main part of the row was clicked
    // * `resultSelType`: A button in the row was clicked ("help" or "dismiss")
    // * "impression_only": No part of the row was clicked
    let eventType;
    if (resultClicked) {
      eventType = "click";
    } else if (resultSelType == "dismiss") {
      eventType = "block";
    } else {
      eventType = resultSelType || "impression_only";
    }

    let suggestion_type;
    switch (result.payload.subtype) {
      case RESULT_SUBTYPE.DYNAMIC_WIKIPEDIA:
        suggestion_type = "dynamic-wikipedia";
        break;
      case RESULT_SUBTYPE.NAVIGATIONAL:
        suggestion_type = "navigational";
        break;
      case RESULT_SUBTYPE.NONSPONSORED:
        suggestion_type = "nonsponsored";
        break;
      case RESULT_SUBTYPE.SPONSORED:
        suggestion_type = "sponsored";
        break;
    }

    Services.telemetry.recordEvent(
      lazy.QuickSuggest.TELEMETRY_EVENT_CATEGORY,
      "engagement",
      eventType,
      "",
      {
        suggestion_type,
        match_type: result.isBestMatch ? "best-match" : "firefox-suggest",
        // Quick suggest telemetry indexes are 1-based but `rowIndex` is 0-based
        position: String(result.rowIndex + 1),
        source: result.payload.source,
      }
    );
  }

  /**
   * Helper for engagement telemetry that records custom contextual services
   * pings.
   *
   * @param {object} options
   *   Options object
   * @param {UrlbarResult} options.result
   *   The quick suggest result related to the engagement. Must not be null.
   * @param {string} options.resultSelType
   *   If an element in the result's row was clicked, this should be its
   *   `selType`. Otherwise it should be an empty string.
   * @param {boolean} options.resultClicked
   *   True if the main part of the result's row was clicked; false if a button
   *   like help or dismiss was clicked or if no part of the row was clicked.
   */
  #recordEngagementPings({ result, resultSelType, resultClicked }) {
    // Custom engagement pings are sent only for the main sponsored and non-
    // sponsored suggestions with an advertiser in their payload, not for other
    // types of suggestions like navigational suggestions.
    if (!result.payload.sponsoredAdvertiser) {
      return;
    }

    let payload = {
      match_type: result.isBestMatch ? "best-match" : "firefox-suggest",
      // Always use lowercase to make the reporting consistent
      advertiser: result.payload.sponsoredAdvertiser.toLocaleLowerCase(),
      block_id: result.payload.sponsoredBlockId,
      improve_suggest_experience_checked: lazy.UrlbarPrefs.get(
        "quicksuggest.dataCollection.enabled"
      ),
      // Quick suggest telemetry indexes are 1-based but `rowIndex` is 0-based
      position: result.rowIndex + 1,
      request_id: result.payload.requestId,
      source: result.payload.source,
    };

    // impression
    lazy.PartnerLinkAttribution.sendContextualServicesPing(
      {
        ...payload,
        is_clicked: resultClicked,
        reporting_url: result.payload.sponsoredImpressionUrl,
      },
      lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION
    );

    // click
    if (resultClicked) {
      lazy.PartnerLinkAttribution.sendContextualServicesPing(
        {
          ...payload,
          reporting_url: result.payload.sponsoredClickUrl,
        },
        lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION
      );
    }

    // dismiss
    if (resultSelType == "dismiss") {
      lazy.PartnerLinkAttribution.sendContextualServicesPing(
        {
          ...payload,
          iab_category: result.payload.sponsoredIabCategory,
        },
        lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK
      );
    }
  }

  /**
   * Helper for engagement telemetry that records telemetry specific to
   * navigational suggestions.
   *
   * @param {object} options
   *   Options object
   * @param {UrlbarQueryContext} options.queryContext
   *   The query context.
   * @param {UrlbarResult} options.result
   *   The quick suggest result related to the engagement, or null if no result
   *   was present.
   * @param {string} options.resultSelType
   *   If an element in the result's row was clicked, this should be its
   *   `selType`. Otherwise it should be an empty string.
   * @param {boolean} options.resultClicked
   *   True if the main part of the result's row was clicked; false if a button
   *   like help or dismiss was clicked or if no part of the row was clicked.
   * @param {object} options.details
   *   The `details` object that was passed to `onEngagement()`. It must look
   *   like this: `{ selType, selIndex }`
   */
  #recordNavSuggestionTelemetry({
    queryContext,
    result,
    resultSelType,
    resultClicked,
    details,
  }) {
    let scalars = [];
    let heuristicClicked =
      details.selIndex == 0 && queryContext.heuristicResult;

    if (result?.payload.subtype == RESULT_SUBTYPE.NAVIGATIONAL) {
      // nav suggestion shown
      scalars.push(TELEMETRY_SCALARS.IMPRESSION_NAV_SHOWN);
      if (resultClicked) {
        scalars.push(TELEMETRY_SCALARS.CLICK_NAV_SHOWN_NAV);
      } else if (heuristicClicked) {
        scalars.push(TELEMETRY_SCALARS.CLICK_NAV_SHOWN_HEURISTIC);
      }
    } else if (
      this.#resultFromLastQuery?.payload.subtype ==
        RESULT_SUBTYPE.NAVIGATIONAL &&
      this.#resultFromLastQuery?.payload.dupedHeuristic
    ) {
      // nav suggestion duped heuristic
      scalars.push(TELEMETRY_SCALARS.IMPRESSION_NAV_SUPERCEDED);
      if (heuristicClicked) {
        scalars.push(TELEMETRY_SCALARS.CLICK_NAV_SUPERCEDED);
      }
    } else {
      // nav suggestion not matched or otherwise not shown
      scalars.push(TELEMETRY_SCALARS.IMPRESSION_NAV_NOTMATCHED);
      if (heuristicClicked) {
        scalars.push(TELEMETRY_SCALARS.CLICK_NAV_NOTMATCHED);
      }
    }

    let heuristicType = UrlbarUtils.searchEngagementTelemetryType(
      queryContext.heuristicResult
    );
    for (let scalar of scalars) {
      Services.telemetry.keyedScalarAdd(scalar, heuristicType, 1);
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
    this.#merino?.cancelTimeoutTimer();

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
    if (!this.#merino) {
      this.#merino = new lazy.MerinoClient(this.name);
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

    let suggestions = await this.#merino.fetch({
      providers,
      query: searchString,
    });

    return suggestions;
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

  get _test_merino() {
    return this.#merino;
  }

  // The result we added during the most recent query.
  #resultFromLastQuery = null;

  // The Merino client.
  #merino = null;
}

export var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
