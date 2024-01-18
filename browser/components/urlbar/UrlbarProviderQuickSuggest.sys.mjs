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
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

// `contextId` is a unique identifier used by Contextual Services
const CONTEXT_ID_PREF = "browser.contextual-services.contextId";
ChromeUtils.defineLazyGetter(lazy, "contextId", () => {
  let _contextId = Services.prefs.getStringPref(CONTEXT_ID_PREF, null);
  if (!_contextId) {
    _contextId = String(Services.uuid.generateUUID());
    Services.prefs.setStringPref(CONTEXT_ID_PREF, _contextId);
  }
  return _contextId;
});

// Used for suggestions that don't otherwise have a score.
const DEFAULT_SUGGESTION_SCORE = 0.2;

const TELEMETRY_PREFIX = "contextual.services.quicksuggest";

const TELEMETRY_SCALARS = {
  BLOCK_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.block_dynamic_wikipedia`,
  BLOCK_NONSPONSORED: `${TELEMETRY_PREFIX}.block_nonsponsored`,
  BLOCK_SPONSORED: `${TELEMETRY_PREFIX}.block_sponsored`,
  CLICK_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.click_dynamic_wikipedia`,
  CLICK_NAV_NOTMATCHED: `${TELEMETRY_PREFIX}.click_nav_notmatched`,
  CLICK_NAV_SHOWN_HEURISTIC: `${TELEMETRY_PREFIX}.click_nav_shown_heuristic`,
  CLICK_NAV_SHOWN_NAV: `${TELEMETRY_PREFIX}.click_nav_shown_nav`,
  CLICK_NAV_SUPERCEDED: `${TELEMETRY_PREFIX}.click_nav_superceded`,
  CLICK_NONSPONSORED: `${TELEMETRY_PREFIX}.click_nonsponsored`,
  CLICK_SPONSORED: `${TELEMETRY_PREFIX}.click_sponsored`,
  HELP_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.help_dynamic_wikipedia`,
  HELP_NONSPONSORED: `${TELEMETRY_PREFIX}.help_nonsponsored`,
  HELP_SPONSORED: `${TELEMETRY_PREFIX}.help_sponsored`,
  IMPRESSION_DYNAMIC_WIKIPEDIA: `${TELEMETRY_PREFIX}.impression_dynamic_wikipedia`,
  IMPRESSION_NAV_NOTMATCHED: `${TELEMETRY_PREFIX}.impression_nav_notmatched`,
  IMPRESSION_NAV_SHOWN: `${TELEMETRY_PREFIX}.impression_nav_shown`,
  IMPRESSION_NAV_SUPERCEDED: `${TELEMETRY_PREFIX}.impression_nav_superceded`,
  IMPRESSION_NONSPONSORED: `${TELEMETRY_PREFIX}.impression_nonsponsored`,
  IMPRESSION_SPONSORED: `${TELEMETRY_PREFIX}.impression_sponsored`,
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
   * @returns {number}
   *   The default score for suggestions that don't otherwise have one. All
   *   suggestions require scores so they can be ranked. Scores are numeric
   *   values in the range [0, 1].
   */
  get DEFAULT_SUGGESTION_SCORE() {
    return DEFAULT_SUGGESTION_SCORE;
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

    return true;
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

    // There are two sources for quick suggest: the current remote settings
    // backend (either JS or Rust) and Merino.
    let promises = [];
    let { backend } = lazy.QuickSuggest;
    if (backend?.isEnabled) {
      promises.push(backend.query(searchString));
    }
    if (
      lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled") &&
      queryContext.allowRemoteResults()
    ) {
      promises.push(this._fetchMerinoSuggestions(queryContext, searchString));
    }

    // Wait for both sources to finish before adding a suggestion.
    let values = await Promise.all(promises);
    if (instance != this.queryInstance) {
      return;
    }

    let suggestions = values.flat();

    // Ensure all suggestions have a `score` by falling back to the default
    // score as necessary. If `quickSuggestScoreMap` is defined, override scores
    // with the values it defines. It maps telemetry types to scores.
    let scoreMap = lazy.UrlbarPrefs.get("quickSuggestScoreMap");
    for (let suggestion of suggestions) {
      if (isNaN(suggestion.score)) {
        suggestion.score = DEFAULT_SUGGESTION_SCORE;
      }
      if (scoreMap) {
        let telemetryType = this.#getSuggestionTelemetryType(suggestion);
        if (scoreMap.hasOwnProperty(telemetryType)) {
          let score = parseFloat(scoreMap[telemetryType]);
          if (!isNaN(score)) {
            suggestion.score = score;
          }
        }
      }
    }

    suggestions.sort((a, b) => b.score - a.score);

    // All suggestions should have the following keys at this point. They are
    // required for looking up the features that manage them.
    let requiredKeys = ["source", "provider"];

    // Add a result for the first suggestion that can be shown.
    for (let suggestion of suggestions) {
      for (let key of requiredKeys) {
        if (!suggestion[key]) {
          this.logger.error(
            `Suggestion is missing required key '${key}': ` +
              JSON.stringify(suggestion)
          );
          continue;
        }
      }

      let canAdd = await this._canAddSuggestion(suggestion);
      if (instance != this.queryInstance) {
        return;
      }

      let result;
      if (
        canAdd &&
        (result = await this.#makeResult(queryContext, suggestion))
      ) {
        this.#resultFromLastQuery = result;
        addCallback(this, result);
        return;
      }
    }
  }

  onEngagement(state, queryContext, details, controller) {
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
        result = this.#getVisibleResultFromLastQuery(controller.view);
      }

      this.#recordEngagement(queryContext, result, details);
    }

    if (details.result?.providerName == this.name) {
      let feature = this.#getFeatureByResult(details.result);
      if (feature?.handleCommand) {
        feature.handleCommand(controller.view, details.result, details.selType);
      } else if (details.selType == "dismiss") {
        // Handle dismissals.
        this.#dismissResult(controller, details.result);
      }
    }

    this.#resultFromLastQuery = null;
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
   *
   * @param {UrlbarResult} result The result whose view will be updated.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result) {
    return this.#getFeatureByResult(result)?.getViewUpdate?.(result);
  }

  getResultCommands(result) {
    return this.#getFeatureByResult(result)?.getResultCommands?.(result);
  }

  /**
   * Gets the `BaseFeature` instance that implements suggestions for a source
   * and provider name. The source and provider name can be supplied from either
   * a suggestion object or the payload of a `UrlbarResult` object.
   *
   * @param {object} options
   *   Options object.
   * @param {string} options.source
   *   The suggestion source, one of: "remote-settings", "merino", "rust"
   * @param {string} options.provider
   *   This value depends on `source`. The possible values per source are:
   *
   *   remote-settings:
   *     The name of the `BaseFeature` instance (`feature.name`) that manages
   *     the suggestion type
   *   merino:
   *     The name of the Merino provider that serves the suggestion type
   *   rust:
   *     The name of the suggestion type as defined in `suggest.udl`
   * @returns {BaseFeature}
   *   The feature instance or null if no feature was found.
   */
  #getFeature({ source, provider }) {
    switch (source) {
      case "remote-settings":
        return lazy.QuickSuggest.getFeature(provider);
      case "merino":
        return lazy.QuickSuggest.getFeatureByMerinoProvider(provider);
      case "rust":
        return lazy.QuickSuggest.getFeatureByRustSuggestionType(provider);
    }
    return null;
  }

  #getFeatureByResult(result) {
    return this.#getFeature(result.payload);
  }

  /**
   * Returns the telemetry type for a suggestion. A telemetry type uniquely
   * identifies a type of suggestion as well as the kind of `UrlbarResult`
   * instances created from it.
   *
   * @param {object} suggestion
   *   A suggestion from remote settings or Merino.
   * @returns {string}
   *   The telemetry type. If the suggestion type is managed by a `BaseFeature`
   *   instance, the telemetry type is retrieved from it. Otherwise the
   *   suggestion type is assumed to come from Merino, and `suggestion.provider`
   *   (the Merino provider name) is returned.
   */
  #getSuggestionTelemetryType(suggestion) {
    let feature = this.#getFeature(suggestion);
    if (feature) {
      return feature.getSuggestionTelemetryType(suggestion);
    }
    return suggestion.provider;
  }

  async #makeResult(queryContext, suggestion) {
    let result;
    let feature = this.#getFeature(suggestion);
    if (!feature) {
      result = this.#makeDefaultResult(queryContext, suggestion);
    } else {
      result = await feature.makeResult(
        queryContext,
        suggestion,
        this._trimmedSearchString
      );
      if (!result) {
        // Feature might return null, if the feature is disabled and so on.
        return null;
      }
    }

    // `source` will be one of: "remote-settings", "merino", "rust".
    // `provider` depends on `source`. See `#getFeature()` for possible values.
    result.payload.source = suggestion.source;
    result.payload.provider = suggestion.provider;
    result.payload.telemetryType = this.#getSuggestionTelemetryType(suggestion);

    // Handle icons here so each feature doesn't have to do it, but use `||=` to
    // let them do it if they need to.
    result.payload.icon ||= suggestion.icon;
    result.payload.iconBlob ||= suggestion.icon_blob;

    // Set the appropriate suggested index and related properties unless the
    // feature did it already.
    if (!result.hasSuggestedIndex) {
      if (suggestion.is_top_pick) {
        result.isBestMatch = true;
        result.isRichSuggestion = true;
        result.richSuggestionIconSize ||= 52;
        result.suggestedIndex = 1;
      } else if (
        suggestion.is_sponsored &&
        lazy.UrlbarPrefs.get("quickSuggestSponsoredPriority")
      ) {
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
    }

    return result;
  }

  #makeDefaultResult(queryContext, suggestion) {
    let payload = {
      url: suggestion.url,
      isSponsored: suggestion.is_sponsored,
      helpUrl: lazy.QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
    };

    if (suggestion.full_keyword) {
      payload.title = suggestion.title;
      payload.qsSuggestion = [
        suggestion.full_keyword,
        UrlbarUtils.HIGHLIGHT.SUGGESTED,
      ];
    } else {
      payload.title = [suggestion.title, UrlbarUtils.HIGHLIGHT.TYPED];
      payload.shouldShowUrl = true;
    }

    return new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );
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

  #dismissResult(controller, result) {
    if (!result.payload.isBlockable) {
      this.logger.info("Dismissals disabled, ignoring dismissal");
      return;
    }

    this.logger.info("Dismissing result: " + JSON.stringify(result));
    lazy.QuickSuggest.blockedSuggestions.add(
      // adM results have `originalUrl`, which contains timestamp templates.
      result.payload.originalUrl ?? result.payload.url
    );
    controller.removeResult(result);
  }

  /**
   * Records engagement telemetry. This should be called only at the end of an
   * engagement when a quick suggest result is present or when a quick suggest
   * result is dismissed.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   * @param {UrlbarResult} result
   *   The quick suggest result that was present (and possibly picked) at the
   *   end of the engagement or that was dismissed. Null if no quick suggest
   *   result was present.
   * @param {object} details
   *   The `details` object that was passed to `onEngagement()`. It must look
   *   like this: `{ selType, selIndex }`
   */
  #recordEngagement(queryContext, result, details) {
    let resultSelType = "";
    let resultClicked = false;
    if (result && details.result == result) {
      resultSelType = details.selType;
      resultClicked =
        details.element?.tagName != "menuitem" &&
        !details.element?.classList.contains("urlbarView-button") &&
        details.selType != "dismiss";
    }

    if (result) {
      // Update impression stats.
      lazy.QuickSuggest.impressionCaps.updateStats(
        result.payload.isSponsored ? "sponsored" : "nonsponsored"
      );

      // Record engagement scalars, event, and pings.
      this.#recordEngagementScalars({ result, resultSelType, resultClicked });
      this.#recordEngagementEvent({ result, resultSelType, resultClicked });
      if (!queryContext.isPrivate) {
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
   * @param {boolean} options.resultClicked
   *   True if the main part of the result's row was clicked; false if a button
   *   like help or dismiss was clicked or if no part of the row was clicked.
   */
  #recordEngagementScalars({ result, resultSelType, resultClicked }) {
    // Navigational suggestion scalars are handled separately.
    if (result.payload.telemetryType == "top_picks") {
      return;
    }

    // Indexes recorded in quick suggest telemetry are 1-based, so add 1 to the
    // 0-based `result.rowIndex`.
    let telemetryResultIndex = result.rowIndex + 1;

    let scalars = [];
    switch (result.payload.telemetryType) {
      case "adm_nonsponsored":
        scalars.push(TELEMETRY_SCALARS.IMPRESSION_NONSPONSORED);
        if (resultClicked) {
          scalars.push(TELEMETRY_SCALARS.CLICK_NONSPONSORED);
        } else {
          switch (resultSelType) {
            case "help":
              scalars.push(TELEMETRY_SCALARS.HELP_NONSPONSORED);
              break;
            case "dismiss":
              scalars.push(TELEMETRY_SCALARS.BLOCK_NONSPONSORED);
              break;
          }
        }
        break;
      case "adm_sponsored":
        scalars.push(TELEMETRY_SCALARS.IMPRESSION_SPONSORED);
        if (resultClicked) {
          scalars.push(TELEMETRY_SCALARS.CLICK_SPONSORED);
        } else {
          switch (resultSelType) {
            case "help":
              scalars.push(TELEMETRY_SCALARS.HELP_SPONSORED);
              break;
            case "dismiss":
              scalars.push(TELEMETRY_SCALARS.BLOCK_SPONSORED);
              break;
          }
        }
        break;
      case "wikipedia":
        scalars.push(TELEMETRY_SCALARS.IMPRESSION_DYNAMIC_WIKIPEDIA);
        if (resultClicked) {
          scalars.push(TELEMETRY_SCALARS.CLICK_DYNAMIC_WIKIPEDIA);
        } else {
          switch (resultSelType) {
            case "help":
              scalars.push(TELEMETRY_SCALARS.HELP_DYNAMIC_WIKIPEDIA);
              break;
            case "dismiss":
              scalars.push(TELEMETRY_SCALARS.BLOCK_DYNAMIC_WIKIPEDIA);
              break;
          }
        }
        break;
    }

    for (let scalar of scalars) {
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
    let eventType;
    if (resultClicked) {
      eventType = "click";
    } else if (!resultSelType) {
      eventType = "impression_only";
    } else {
      switch (resultSelType) {
        case "dismiss":
          eventType = "block";
          break;
        case "help":
          eventType = "help";
          break;
        default:
          eventType = "other";
          break;
      }
    }

    let suggestion_type;
    switch (result.payload.telemetryType) {
      case "adm_nonsponsored":
        suggestion_type = "nonsponsored";
        break;
      case "adm_sponsored":
        suggestion_type = "sponsored";
        break;
      case "top_picks":
        suggestion_type = "navigational";
        break;
      case "wikipedia":
        suggestion_type = "dynamic-wikipedia";
        break;
      default:
        suggestion_type = result.payload.telemetryType;
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
    if (
      result.payload.telemetryType != "adm_sponsored" &&
      result.payload.telemetryType != "adm_nonsponsored"
    ) {
      return;
    }

    // Contextual services ping paylod
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
      suggested_index: result.suggestedIndex,
      suggested_index_relative_to_group:
        !!result.isSuggestedIndexRelativeToGroup,
      request_id: result.payload.requestId,
      source: result.payload.source,
    };

    // Glean ping key -> value
    let defaultValuesByGleanKey = {
      matchType: payload.match_type,
      advertiser: payload.advertiser,
      blockId: payload.block_id,
      improveSuggestExperience: payload.improve_suggest_experience_checked,
      position: payload.position,
      suggestedIndex: payload.suggested_index.toString(),
      suggestedIndexRelativeToGroup: payload.suggested_index_relative_to_group,
      requestId: payload.request_id,
      source: payload.source,
      contextId: lazy.contextId,
    };

    let sendGleanPing = valuesByGleanKey => {
      valuesByGleanKey = { ...defaultValuesByGleanKey, ...valuesByGleanKey };
      for (let [gleanKey, value] of Object.entries(valuesByGleanKey)) {
        let glean = Glean.quickSuggest[gleanKey];
        if (value !== undefined && value !== "") {
          glean.set(value);
        }
      }
      GleanPings.quickSuggest.submit();
    };

    // impression
    sendGleanPing({
      pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
      isClicked: resultClicked,
      reportingUrl: result.payload.sponsoredImpressionUrl,
    });

    // click
    if (resultClicked) {
      sendGleanPing({
        pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
        reportingUrl: result.payload.sponsoredClickUrl,
      });
    }

    // dismiss
    if (resultSelType == "dismiss") {
      sendGleanPing({
        pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
        iabCategory: result.payload.sponsoredIabCategory,
      });
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

    if (result?.payload.telemetryType == "top_picks") {
      // nav suggestion shown
      scalars.push(TELEMETRY_SCALARS.IMPRESSION_NAV_SHOWN);
      if (resultClicked) {
        scalars.push(TELEMETRY_SCALARS.CLICK_NAV_SHOWN_NAV);
      } else if (heuristicClicked) {
        scalars.push(TELEMETRY_SCALARS.CLICK_NAV_SHOWN_HEURISTIC);
      }
    } else if (
      this.#resultFromLastQuery?.payload.telemetryType == "top_picks" &&
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
    // Cancel the Rust query.
    let backend = lazy.QuickSuggest.getFeature("SuggestBackendRust");
    if (backend?.isEnabled) {
      backend.cancelQuery();
    }

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

    // Return false if the suggestion is blocked based on its URL. Suggestions
    // from the JS backend define a single `url` property. Suggestions from the
    // Rust backend are more complicated: Sponsored suggestions define `rawUrl`,
    // which may contain timestamp templates, while non-sponsored suggestions
    // define only `url`. Blocking should always be based on URLs with timestamp
    // templates, where applicable, so check `rawUrl` and then `url`, in that
    // order.
    let { blockedSuggestions } = lazy.QuickSuggest;
    if (await blockedSuggestions.has(suggestion.rawUrl ?? suggestion.url)) {
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
