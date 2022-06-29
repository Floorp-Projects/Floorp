/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderQuickSuggest", "QUICK_SUGGEST_SOURCE"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  clearInterval: "resource://gre/modules/Timer.jsm",
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  setInterval: "resource://gre/modules/Timer.jsm",
  SkippableTimer: "resource:///modules/UrlbarUtils.jsm",
  TaskQueue: "resource:///modules/UrlbarUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

const TIMESTAMP_TEMPLATE = "%YYYYMMDDHH%";
const TIMESTAMP_LENGTH = 10;
const TIMESTAMP_REGEXP = /^\d{10}$/;

const MERINO_PARAMS = {
  CLIENT_VARIANTS: "client_variants",
  PROVIDERS: "providers",
  QUERY: "q",
  SEQUENCE_NUMBER: "seq",
  SESSION_ID: "sid",
};

const MERINO_SESSION_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes

const IMPRESSION_COUNTERS_RESET_INTERVAL_MS = 60 * 60 * 1000; // 1 hour

const TELEMETRY_MERINO_LATENCY = "FX_URLBAR_MERINO_LATENCY_MS";
const TELEMETRY_MERINO_RESPONSE = "FX_URLBAR_MERINO_RESPONSE";

const TELEMETRY_REMOTE_SETTINGS_LATENCY =
  "FX_URLBAR_QUICK_SUGGEST_REMOTE_SETTINGS_LATENCY_MS";

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

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

// This object maps impression stats object keys to their corresponding keys in
// the `extra` object of impression cap telemetry events. The main reason this
// is necessary is because the keys of the `extra` object are limited to 15
// characters in length, which some stats object keys exceed. It also forces us
// to be deliberate about keys we add to the `extra` object, since the `extra`
// object is limited to 10 keys.
let TELEMETRY_IMPRESSION_CAP_EXTRA_KEYS = {
  // stats object key -> `extra` telemetry event object key
  intervalSeconds: "intervalSeconds",
  startDateMs: "startDate",
  count: "count",
  maxCount: "maxCount",
  impressionDateMs: "impressionDate",
};

// Identifies the source of the QuickSuggest suggestion.
const QUICK_SUGGEST_SOURCE = {
  REMOTE_SETTINGS: "remote-settings",
  MERINO: "merino",
};

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderQuickSuggest extends UrlbarProvider {
  constructor(...args) {
    super(...args);

    UrlbarQuickSuggest.init();
    UrlbarQuickSuggest.on("config-set", () => this._validateImpressionStats());

    this._updateFeatureState();
    NimbusFeatures.urlbar.onUpdate(() => this._updateFeatureState());

    UrlbarPrefs.addObserver(this);

    // Periodically record impression counters reset telemetry.
    this._setImpressionCountersResetInterval();

    // On shutdown, record any final impression counters reset telemetry.
    AsyncShutdown.profileChangeTeardown.addBlocker(
      "UrlbarProviderQuickSuggest: Record impression counters reset telemetry",
      () => this._resetElapsedImpressionCounters()
    );
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "UrlbarProviderQuickSuggest";
  }

  /**
   * The type of the provider.
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.NETWORK;
  }

  /**
   * @returns {string} The help URL for the Quick Suggest feature.
   */
  get helpUrl() {
    return (
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "firefox-suggest"
    );
  }

  /**
   * @returns {string} The help URL for the Quick Suggest best match feature.
   */
  get bestMatchHelpUrl() {
    return this.helpUrl;
  }

  /**
   * @returns {string} The timestamp template string used in quick suggest URLs.
   */
  get TIMESTAMP_TEMPLATE() {
    return TIMESTAMP_TEMPLATE;
  }

  /**
   * @returns {number} The length of the timestamp in quick suggest URLs.
   */
  get TIMESTAMP_LENGTH() {
    return TIMESTAMP_LENGTH;
  }

  /**
   * @returns {object} An object mapping from mnemonics to scalar names.
   */
  get TELEMETRY_SCALARS() {
    return { ...TELEMETRY_SCALARS };
  }

  /**
   * @returns {object} An object mapping from mnemonics to Merino search params.
   */
  get MERINO_PARAMS() {
    return { ...MERINO_PARAMS };
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
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
    return (
      queryContext.trimmedSearchString &&
      !queryContext.searchMode &&
      !queryContext.isPrivate &&
      UrlbarPrefs.get("quickSuggestEnabled") &&
      (UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") ||
        UrlbarPrefs.get("suggest.quicksuggest.sponsored") ||
        UrlbarPrefs.get("quicksuggest.dataCollection.enabled"))
    );
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   */
  async startQuery(queryContext, addCallback) {
    let instance = this.queryInstance;

    // Trim only the start of the search string because a trailing space can
    // affect the suggestions.
    let searchString = queryContext.searchString.trimStart();

    // There are two sources for quick suggest: remote settings (from
    // `UrlbarQuickSuggest`) and Merino.
    let promises = [];
    if (UrlbarPrefs.get("quickSuggestRemoteSettingsEnabled")) {
      promises.push(
        this._fetchRemoteSettingsSuggestions(queryContext, searchString)
      );
    }
    if (
      UrlbarPrefs.get("merinoEnabled") &&
      UrlbarPrefs.get("quicksuggest.dataCollection.enabled") &&
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
    this._replaceSuggestionTemplates(suggestion);

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
      helpUrl: this.helpUrl,
      helpL10nId: "firefox-suggest-urlbar-learn-more",
      source: suggestion.source,
      requestId: suggestion.request_id,
    };

    // Determine if the suggestion itself is a best match.
    let isSuggestionBestMatch = false;
    if (typeof suggestion._test_is_best_match == "boolean") {
      isSuggestionBestMatch = suggestion._test_is_best_match;
    } else if (UrlbarQuickSuggest.config.best_match) {
      let { best_match } = UrlbarQuickSuggest.config;
      isSuggestionBestMatch =
        best_match.min_search_string_length <= searchString.length &&
        !best_match.blocked_suggestion_ids.includes(suggestion.block_id);
    }

    // Determine if the urlbar result should be a best match.
    let isResultBestMatch =
      isSuggestionBestMatch &&
      UrlbarPrefs.get("bestMatchEnabled") &&
      UrlbarPrefs.get("suggest.bestmatch");
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

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
    );

    if (isResultBestMatch) {
      result.isBestMatch = true;
      result.suggestedIndex = 1;
    } else if (
      !isNaN(suggestion.position) &&
      UrlbarPrefs.get("quickSuggestAllowPositionInSuggestions")
    ) {
      result.suggestedIndex = suggestion.position;
    } else {
      result.isSuggestedIndexRelativeToGroup = true;
      result.suggestedIndex = UrlbarPrefs.get(
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
      UrlbarPrefs.get("isBestMatchExperiment") ||
      UrlbarPrefs.get("experimentType") === "best-match"
    ) {
      if (
        isSuggestionBestMatch &&
        (!UrlbarPrefs.get("bestMatchEnabled") ||
          UrlbarPrefs.get("suggest.bestmatch"))
      ) {
        UrlbarQuickSuggest.ensureExposureEventRecorded();
      }
    } else if (UrlbarPrefs.get("experimentType") !== "modal") {
      UrlbarQuickSuggest.ensureExposureEventRecorded();
    }
  }

  /**
   * Called when the result's block button is picked. If the provider can block
   * the result, it should do so and return true. If the provider cannot block
   * the result, it should return false. The meaning of "blocked" depends on the
   * provider and the type of result.
   *
   * @param {UrlbarQueryContext} queryContext
   * @param {UrlbarResult} result
   *   The result that should be blocked.
   * @returns {boolean}
   *   Whether the result was blocked.
   */
  blockResult(queryContext, result) {
    if (
      (!result.isBestMatch &&
        !UrlbarPrefs.get("quickSuggestBlockingEnabled")) ||
      (result.isBestMatch && !UrlbarPrefs.get("bestMatchBlockingEnabled"))
    ) {
      this.logger.info("Blocking disabled, ignoring block");
      return false;
    }

    this.logger.info("Blocking result: " + JSON.stringify(result));
    this.blockSuggestion(result.payload.originalUrl);
    this._recordEngagementTelemetry(result, queryContext.isPrivate, "block");
    return true;
  }

  /**
   * Blocks a suggestion.
   *
   * @param {string} originalUrl
   *   The suggestion's original URL with its unreplaced timestamp template.
   */
  async blockSuggestion(originalUrl) {
    this.logger.debug(`Queueing blockSuggestion: ${originalUrl}`);
    await this._blockTaskQueue.queue(async () => {
      this.logger.info(`Blocking suggestion: ${originalUrl}`);
      let digest = await this._getDigest(originalUrl);
      this.logger.debug(`Got digest for '${originalUrl}': ${digest}`);
      this._blockedDigests.add(digest);
      let json = JSON.stringify([...this._blockedDigests]);
      this._updatingBlockedDigests = true;
      try {
        UrlbarPrefs.set("quicksuggest.blockedDigests", json);
      } finally {
        this._updatingBlockedDigests = false;
      }
      this.logger.debug(`All blocked suggestions: ${json}`);
    });
  }

  /**
   * Gets whether a suggestion is blocked.
   *
   * @param {string} originalUrl
   *   The suggestion's original URL with its unreplaced timestamp template.
   * @returns {boolean}
   *   Whether the suggestion is blocked.
   */
  async isSuggestionBlocked(originalUrl) {
    this.logger.debug(`Queueing isSuggestionBlocked: ${originalUrl}`);
    return this._blockTaskQueue.queue(async () => {
      this.logger.info(`Getting blocked status: ${originalUrl}`);
      let digest = await this._getDigest(originalUrl);
      this.logger.debug(`Got digest for '${originalUrl}': ${digest}`);
      let isBlocked = this._blockedDigests.has(digest);
      this.logger.info(`Blocked status for '${originalUrl}': ${isBlocked}`);
      return isBlocked;
    });
  }

  /**
   * Unblocks all suggestions.
   */
  async clearBlockedSuggestions() {
    this.logger.debug(`Queueing clearBlockedSuggestions`);
    await this._blockTaskQueue.queue(() => {
      this.logger.info(`Clearing all blocked suggestions`);
      this._blockedDigests.clear();
      UrlbarPrefs.clear("quicksuggest.blockedDigests");
    });
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
    if (this._merinoSessionID && state != "start") {
      this._resetMerinoSessionID();
    }

    // Per spec, we count impressions only when the user picks a result, i.e.,
    // when `state` is "engagement".
    if (result && state == "engagement") {
      this._recordEngagementTelemetry(
        result,
        isPrivate,
        details.selIndex == result.rowIndex ? details.selType : ""
      );
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
    this._updateImpressionStats(result.payload.isSponsored);

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
      TELEMETRY_EVENT_CATEGORY,
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
        improve_suggest_experience_checked: UrlbarPrefs.get(
          "quicksuggest.dataCollection.enabled"
        ),
        position: telemetryResultIndex,
        request_id: result.payload.requestId,
      };

      // impression
      PartnerLinkAttribution.sendContextualServicesPing(
        {
          ...payload,
          is_clicked,
          reporting_url: result.payload.sponsoredImpressionUrl,
        },
        CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION
      );

      // click
      if (is_clicked) {
        PartnerLinkAttribution.sendContextualServicesPing(
          {
            ...payload,
            reporting_url: result.payload.sponsoredClickUrl,
          },
          CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION
        );
      }

      // block
      if (selType == "block") {
        PartnerLinkAttribution.sendContextualServicesPing(
          {
            ...payload,
            iab_category: result.payload.sponsoredIabCategory,
          },
          CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK
        );
      }
    }
  }

  /**
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "quicksuggest.blockedDigests":
        if (!this._updatingBlockedDigests) {
          this.logger.info(
            "browser.urlbar.quicksuggest.blockedDigests changed"
          );
          this._loadBlockedDigests();
        }
        break;
      case "quicksuggest.impressionCaps.stats":
        if (!this._updatingImpressionStats) {
          this.logger.info(
            "browser.urlbar.quicksuggest.impressionCaps.stats changed"
          );
          this._loadImpressionStats();
        }
        break;
      case "quicksuggest.dataCollection.enabled":
        if (!UrlbarPrefs.updatingFirefoxSuggestScenario) {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            "data_collect_toggled",
            UrlbarPrefs.get(pref) ? "enabled" : "disabled"
          );
        }
        break;
      case "suggest.quicksuggest.nonsponsored":
        if (!UrlbarPrefs.updatingFirefoxSuggestScenario) {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            "enable_toggled",
            UrlbarPrefs.get(pref) ? "enabled" : "disabled"
          );
        }
        break;
      case "suggest.quicksuggest.sponsored":
        if (!UrlbarPrefs.updatingFirefoxSuggestScenario) {
          Services.telemetry.recordEvent(
            TELEMETRY_EVENT_CATEGORY,
            "sponsored_toggled",
            UrlbarPrefs.get(pref) ? "enabled" : "disabled"
          );
        }
        break;
    }
  }

  /**
   * Cancels the current query.
   *
   * @param {UrlbarQueryContext} queryContext
   */
  cancelQuery(queryContext) {
    // Cancel the Merino timeout timer so it doesn't fire and record a timeout.
    // If it's already canceled or has fired, this is a no-op.
    this._merinoTimeoutTimer?.cancel();

    // Don't abort the Merino fetch if one is ongoing. By design we allow
    // fetches to finish so we can record their latency.
  }

  /**
   * Returns whether a given URL and quick suggest's URL are equivalent. URLs
   * are equivalent if they are identical except for substrings that replaced
   * templates in the original suggestion URL.
   *
   * For example, a suggestion URL from the backing suggestions source might
   * include a timestamp template "%YYYYMMDDHH%" like this:
   *
   *   http://example.com/foo?bar=%YYYYMMDDHH%
   *
   * When a quick suggest result is created from this suggestion URL, it's
   * created with a URL that is a copy of the suggestion URL but with the
   * template replaced with a real timestamp value, like this:
   *
   *   http://example.com/foo?bar=2021111610
   *
   * All URLs created from this single suggestion URL are considered equivalent
   * regardless of their real timestamp values.
   *
   * @param {string} url
   * @param {UrlbarResult} result
   * @returns {boolean}
   *   Whether `url` is equivalent to `result.payload.url`.
   */
  isURLEquivalentToResultURL(url, result) {
    // If the URLs aren't the same length, they can't be equivalent.
    let resultURL = result.payload.url;
    if (resultURL.length != url.length) {
      return false;
    }

    // If the result URL doesn't have a timestamp, then do a straight string
    // comparison.
    let { urlTimestampIndex } = result.payload;
    if (typeof urlTimestampIndex != "number" || urlTimestampIndex < 0) {
      return resultURL == url;
    }

    // Compare the first parts of the strings before the timestamps.
    if (
      resultURL.substring(0, urlTimestampIndex) !=
      url.substring(0, urlTimestampIndex)
    ) {
      return false;
    }

    // Compare the second parts of the strings after the timestamps.
    let remainderIndex = urlTimestampIndex + TIMESTAMP_LENGTH;
    if (resultURL.substring(remainderIndex) != url.substring(remainderIndex)) {
      return false;
    }

    // Test the timestamp against the regexp.
    let maybeTimestamp = url.substring(
      urlTimestampIndex,
      urlTimestampIndex + TIMESTAMP_LENGTH
    );
    return TIMESTAMP_REGEXP.test(maybeTimestamp);
  }

  /**
   * Fetches remote settings suggestions.
   *
   * @param {UrlbarQueryContext} queryContext
   * @param {string} searchString
   * @returns {array}
   *   The remote settings suggestions. If there are no matches, an empty array
   *   is returned.
   */
  async _fetchRemoteSettingsSuggestions(queryContext, searchString) {
    let instance = this.queryInstance;

    let suggestions;
    TelemetryStopwatch.start(TELEMETRY_REMOTE_SETTINGS_LATENCY, queryContext);
    try {
      suggestions = await UrlbarQuickSuggest.query(searchString);
      TelemetryStopwatch.finish(
        TELEMETRY_REMOTE_SETTINGS_LATENCY,
        queryContext
      );
      if (instance != this.queryInstance) {
        return [];
      }
    } catch (error) {
      TelemetryStopwatch.cancel(
        TELEMETRY_REMOTE_SETTINGS_LATENCY,
        queryContext
      );
      this.logger.error("Couldn't fetch remote settings suggestions: " + error);
    }

    return suggestions || [];
  }

  /**
   * Fetches Merino suggestions.
   *
   * @param {UrlbarQueryContext} queryContext
   * @param {string} searchString
   * @returns {array}
   *   The Merino suggestions or null if there's an error or unexpected
   *   response.
   */
  async _fetchMerinoSuggestions(queryContext, searchString) {
    let instance = this.queryInstance;

    // Set up the Merino session ID and related state.
    if (!this._merinoSessionID) {
      this._merinoSessionID = Services.uuid.generateUUID();
      this._merinoSequenceNumber = 0;
      this._merinoSessionTimer?.cancel();

      // Per spec, for the user's privacy, the session should time out and a new
      // session ID should be used if the engagement does not end soon.
      this._merinoSessionTimer = new SkippableTimer({
        name: "Merino session timeout",
        time: this._merinoSessionTimeoutMs,
        logger: this.logger,
        callback: () => this._resetMerinoSessionID(),
      });
    }

    // Get the endpoint URL. It's empty by default when running tests so they
    // don't hit the network.
    let endpointString = UrlbarPrefs.get("merino.endpointURL");
    if (!endpointString) {
      return null;
    }
    let url;
    try {
      url = new URL(endpointString);
    } catch (error) {
      this.logger.error("Could not make Merino endpoint URL: " + error);
      return null;
    }
    url.searchParams.set(MERINO_PARAMS.QUERY, searchString);
    url.searchParams.set(MERINO_PARAMS.SESSION_ID, this._merinoSessionID);
    url.searchParams.set(
      MERINO_PARAMS.SEQUENCE_NUMBER,
      this._merinoSequenceNumber
    );

    let clientVariants = UrlbarPrefs.get("merino.clientVariants");
    if (clientVariants) {
      url.searchParams.set(MERINO_PARAMS.CLIENT_VARIANTS, clientVariants);
    }

    let providers = UrlbarPrefs.get("merino.providers");
    if (providers) {
      url.searchParams.set(MERINO_PARAMS.PROVIDERS, providers);
    } else if (
      !UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") &&
      !UrlbarPrefs.get("suggest.quicksuggest.sponsored")
    ) {
      // Data collection is enabled but suggestions are not. Set the providers
      // param to an empty string to tell Merino not to fetch any suggestions.
      url.searchParams.set(MERINO_PARAMS.PROVIDERS, "");
    }

    let responseHistogram = Services.telemetry.getHistogramById(
      TELEMETRY_MERINO_RESPONSE
    );
    let maybeRecordResponse = category => {
      responseHistogram?.add(category);
      responseHistogram = null;
    };

    // Set up the timeout timer.
    let timeout = UrlbarPrefs.get("merinoTimeoutMs");
    let timer = (this._merinoTimeoutTimer = new SkippableTimer({
      name: "Merino timeout",
      time: timeout,
      logger: this.logger,
      callback: () => {
        // The fetch timed out.
        this.logger.info(`Merino fetch timed out (timeout = ${timeout}ms)`);
        maybeRecordResponse("timeout");
      },
    }));

    // If there's an ongoing fetch, abort it so there's only one at a time. By
    // design we do not abort fetches on timeout or when the query is canceled
    // so we can record their latency.
    try {
      this._merinoFetchController?.abort();
    } catch (error) {
      this.logger.error("Could not abort Merino fetch: " + error);
    }

    // Do the fetch.
    let response;
    let controller = (this._merinoFetchController = new AbortController());
    TelemetryStopwatch.start(TELEMETRY_MERINO_LATENCY, queryContext);
    await Promise.race([
      timer.promise,
      (async () => {
        try {
          // Canceling the timer below resolves its promise, which can resolve
          // the outer promise created by `Promise.race`. This inner async
          // function happens not to await anything after canceling the timer,
          // but if it did, `timer.promise` could win the race and resolve the
          // outer promise without a value. For that reason, we declare
          // `response` in the outer scope and set it here instead of returning
          // the response from this inner function and assuming it will also be
          // returned by `Promise.race`.
          response = await fetch(url, { signal: controller.signal });
          TelemetryStopwatch.finish(TELEMETRY_MERINO_LATENCY, queryContext);
          maybeRecordResponse(response.ok ? "success" : "http_error");

          // Increment the sequence number only after the fetch successfully
          // completes. It should not be incremented if the fetch is aborted or
          // fails due to a network error. The server should not see gaps in
          // sequence numbers for searches it never received. In particular, as
          // the user quickly types a search string and we start a search after
          // each new character, some of those searches may cancel previous ones
          // before their fetches complete or even start.
          this._merinoSequenceNumber++;
        } catch (error) {
          TelemetryStopwatch.cancel(TELEMETRY_MERINO_LATENCY, queryContext);
          if (error.name != "AbortError") {
            this.logger.error("Could not fetch Merino endpoint: " + error);
            maybeRecordResponse("network_error");
          }
        } finally {
          // Now that the fetch is done, cancel the timeout timer so it doesn't
          // fire and record a timeout. If it already fired, which it would have
          // on timeout, or was already canceled, this is a no-op.
          timer.cancel();
          if (controller == this._merinoFetchController) {
            this._merinoFetchController = null;
          }
        }
      })(),
    ]);
    if (timer == this._merinoTimeoutTimer) {
      this._merinoTimeoutTimer = null;
    }
    if (instance != this.queryInstance) {
      return null;
    }

    // Get the response body as an object.
    let body;
    try {
      body = await response?.json();
      if (instance != this.queryInstance) {
        return null;
      }
    } catch (error) {
      this.logger.error("Could not get Merino response as JSON: " + error);
    }

    if (!body?.suggestions?.length) {
      return null;
    }

    let { suggestions, request_id } = body;
    if (!Array.isArray(suggestions)) {
      this.logger.error("Unexpected Merino response: " + JSON.stringify(body));
      return null;
    }

    return suggestions.map(suggestion => ({
      ...suggestion,
      request_id,
      source: QUICK_SUGGEST_SOURCE.MERINO,
    }));
  }

  /**
   * Resets the Merino session ID and related state.
   */
  _resetMerinoSessionID() {
    this._merinoSessionID = null;
    this._merinoSequenceNumber = 0;
    this._merinoSessionTimer?.cancel();
    this._merinoSessionTimer = null;
  }

  /**
   * Returns whether a given suggestion can be added for a query, assuming the
   * provider itself should be active.
   *
   * @param {object} suggestion
   * @returns {boolean}
   *   Whether the suggestion can be added.
   */
  async _canAddSuggestion(suggestion) {
    this.logger.info("Checking if suggestion can be added");
    this.logger.debug(JSON.stringify({ suggestion }));

    // Return false if suggestions are disabled.
    if (
      (suggestion.is_sponsored &&
        !UrlbarPrefs.get("suggest.quicksuggest.sponsored")) ||
      (!suggestion.is_sponsored &&
        !UrlbarPrefs.get("suggest.quicksuggest.nonsponsored"))
    ) {
      this.logger.info("Suggestions disabled, not adding suggestion");
      return false;
    }

    // Return false if an impression cap has been hit.
    if (
      (suggestion.is_sponsored &&
        UrlbarPrefs.get("quickSuggestImpressionCapsSponsoredEnabled")) ||
      (!suggestion.is_sponsored &&
        UrlbarPrefs.get("quickSuggestImpressionCapsNonSponsoredEnabled"))
    ) {
      this._resetElapsedImpressionCounters();
      let type = suggestion.is_sponsored ? "sponsored" : "nonsponsored";
      let stats = this._impressionStats[type];
      if (stats) {
        let hitStats = stats.filter(s => s.maxCount <= s.count);
        if (hitStats.length) {
          this.logger.info("Impression cap(s) hit, not adding suggestion");
          this.logger.debug(JSON.stringify({ type, hitStats }));
          return false;
        }
      }
    }

    // Return false if the suggestion is blocked.
    if (await this.isSuggestionBlocked(suggestion.url)) {
      this.logger.info("Suggestion blocked, not adding suggestion");
      return false;
    }

    this.logger.info("Suggestion can be added");
    return true;
  }

  /**
   * Some suggestion properties like `url` and `click_url` include template
   * substrings that must be replaced with real values. This method replaces
   * templates with appropriate values in place.
   *
   * @param {object} suggestion
   *   A suggestion object fetched from remote settings or Merino.
   */
  _replaceSuggestionTemplates(suggestion) {
    let now = new Date();
    let timestampParts = [
      now.getFullYear(),
      now.getMonth() + 1,
      now.getDate(),
      now.getHours(),
    ];
    let timestamp = timestampParts
      .map(n => n.toString().padStart(2, "0"))
      .join("");
    for (let key of ["url", "click_url"]) {
      let value = suggestion[key];
      if (!value) {
        continue;
      }

      let timestampIndex = value.indexOf(TIMESTAMP_TEMPLATE);
      if (timestampIndex >= 0) {
        if (key == "url") {
          suggestion.urlTimestampIndex = timestampIndex;
        }
        // We could use replace() here but we need the timestamp index for
        // `suggestion.urlTimestampIndex`, and since we already have that, avoid
        // another O(n) substring search and manually replace the template with
        // the timestamp.
        suggestion[key] =
          value.substring(0, timestampIndex) +
          timestamp +
          value.substring(timestampIndex + TIMESTAMP_TEMPLATE.length);
      }
    }
  }

  /**
   * Increments the user's impression stats counters for the given type of
   * suggestion. This should be called only when a suggestion impression is
   * recorded.
   *
   * @param {boolean} isSponsored
   *   Whether the impression was recorded for a sponsored suggestion.
   */
  _updateImpressionStats(isSponsored) {
    this.logger.info("Starting impression stats update");
    this.logger.debug(
      JSON.stringify({
        isSponsored,
        currentStats: this._impressionStats,
        impression_caps: UrlbarQuickSuggest.config.impression_caps,
      })
    );

    // Don't bother recording anything if caps are disabled.
    if (
      (isSponsored &&
        !UrlbarPrefs.get("quickSuggestImpressionCapsSponsoredEnabled")) ||
      (!isSponsored &&
        !UrlbarPrefs.get("quickSuggestImpressionCapsNonSponsoredEnabled"))
    ) {
      this.logger.info("Impression caps disabled, skipping update");
      return;
    }

    // Get the user's impression stats. Since stats are synced from caps, if the
    // stats don't exist then the caps don't exist, and don't bother recording
    // anything in that case.
    let type = isSponsored ? "sponsored" : "nonsponsored";
    let stats = this._impressionStats[type];
    if (!stats) {
      this.logger.info("Impression caps undefined, skipping update");
      return;
    }

    // Increment counters.
    for (let stat of stats) {
      stat.count++;
      stat.impressionDateMs = Date.now();

      // Record a telemetry event for each newly hit cap.
      if (stat.count == stat.maxCount) {
        this.logger.info(`'${type}' impression cap hit`);
        this.logger.debug(JSON.stringify({ type, hitStat: stat }));
        this._recordImpressionCapEvent({
          stat,
          eventType: "hit",
          suggestionType: type,
        });
      }
    }

    // Save the stats.
    this._updatingImpressionStats = true;
    try {
      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify(this._impressionStats)
      );
    } finally {
      this._updatingImpressionStats = false;
    }

    this.logger.info("Finished impression stats update");
    this.logger.debug(JSON.stringify({ newStats: this._impressionStats }));
  }

  /**
   * Loads and validates impression stats.
   */
  _loadImpressionStats() {
    let json = UrlbarPrefs.get("quicksuggest.impressionCaps.stats");
    if (!json) {
      this._impressionStats = {};
    } else {
      try {
        this._impressionStats = JSON.parse(
          json,
          // Infinity, which is the `intervalSeconds` for the lifetime cap, is
          // stringified as `null` in the JSON, so convert it back to Infinity.
          (key, value) =>
            key == "intervalSeconds" && value === null ? Infinity : value
        );
      } catch (error) {}
    }
    this._validateImpressionStats();
  }

  /**
   * Validates impression stats, which includes two things:
   *
   * - Type checks stats and discards any that are invalid. We do this because
   *   stats are stored in prefs where anyone can modify them.
   * - Syncs stats with impression caps so that there is one stats object
   *   corresponding to each impression cap. See the `_impressionStats` comment
   *   for more info.
   */
  _validateImpressionStats() {
    let { impression_caps } = UrlbarQuickSuggest.config;

    this.logger.info("Validating impression stats");
    this.logger.debug(
      JSON.stringify({
        impression_caps,
        currentStats: this._impressionStats,
      })
    );

    if (!this._impressionStats || typeof this._impressionStats != "object") {
      this._impressionStats = {};
    }

    for (let [type, cap] of Object.entries(impression_caps || {})) {
      // Build a map from interval seconds to max counts in the caps.
      let maxCapCounts = (cap.custom || []).reduce(
        (map, { interval_s, max_count }) => {
          map.set(interval_s, max_count);
          return map;
        },
        new Map()
      );
      if (typeof cap.lifetime == "number") {
        maxCapCounts.set(Infinity, cap.lifetime);
      }

      let stats = this._impressionStats[type];
      if (!Array.isArray(stats)) {
        stats = [];
        this._impressionStats[type] = stats;
      }

      // Validate existing stats:
      //
      // * Discard stats with invalid properties.
      // * Collect and remove stats with intervals that aren't in the caps. This
      //   should only happen when caps are changed or removed.
      // * For stats with intervals that are in the caps:
      //   * Keep track of the max `stat.count` across all stats so we can
      //     update the lifetime stat below.
      //   * Set `stat.maxCount` to the max count in the corresponding cap.
      let orphanStats = [];
      let maxCountInStats = 0;
      for (let i = 0; i < stats.length; ) {
        let stat = stats[i];
        if (
          typeof stat.intervalSeconds != "number" ||
          typeof stat.startDateMs != "number" ||
          typeof stat.count != "number" ||
          typeof stat.maxCount != "number" ||
          typeof stat.impressionDateMs != "number"
        ) {
          stats.splice(i, 1);
        } else {
          maxCountInStats = Math.max(maxCountInStats, stat.count);
          let maxCount = maxCapCounts.get(stat.intervalSeconds);
          if (maxCount === undefined) {
            stats.splice(i, 1);
            orphanStats.push(stat);
          } else {
            stat.maxCount = maxCount;
            i++;
          }
        }
      }

      // Create stats for caps that don't already have corresponding stats.
      for (let [intervalSeconds, maxCount] of maxCapCounts.entries()) {
        if (!stats.some(s => s.intervalSeconds == intervalSeconds)) {
          stats.push({
            maxCount,
            intervalSeconds,
            startDateMs: Date.now(),
            count: 0,
            impressionDateMs: 0,
          });
        }
      }

      // Merge orphaned stats into other ones if possible. For each orphan, if
      // its interval is no bigger than an existing stat's interval, then the
      // orphan's count can contribute to the existing stat's count, so merge
      // the two.
      for (let orphan of orphanStats) {
        for (let stat of stats) {
          if (orphan.intervalSeconds <= stat.intervalSeconds) {
            stat.count = Math.max(stat.count, orphan.count);
            stat.startDateMs = Math.min(stat.startDateMs, orphan.startDateMs);
            stat.impressionDateMs = Math.max(
              stat.impressionDateMs,
              orphan.impressionDateMs
            );
          }
        }
      }

      // If the lifetime stat exists, make its count the max count found above.
      // This is only necessary when the lifetime cap wasn't present before, but
      // it doesn't hurt to always do it.
      let lifetimeStat = stats.find(s => s.intervalSeconds == Infinity);
      if (lifetimeStat) {
        lifetimeStat.count = maxCountInStats;
      }

      // Sort the stats by interval ascending. This isn't necessary except that
      // it guarantees an ordering for tests.
      stats.sort((a, b) => a.intervalSeconds - b.intervalSeconds);
    }

    this.logger.debug(JSON.stringify({ newStats: this._impressionStats }));
  }

  /**
   * Resets the counters of impression stats whose intervals have elapased.
   */
  _resetElapsedImpressionCounters() {
    this.logger.info("Checking for elapsed impression cap intervals");
    this.logger.debug(
      JSON.stringify({
        currentStats: this._impressionStats,
        impression_caps: UrlbarQuickSuggest.config.impression_caps,
      })
    );

    let now = Date.now();
    for (let [type, stats] of Object.entries(this._impressionStats)) {
      for (let stat of stats) {
        let elapsedMs = now - stat.startDateMs;
        let intervalMs = 1000 * stat.intervalSeconds;
        let elapsedIntervalCount = Math.floor(elapsedMs / intervalMs);
        if (elapsedIntervalCount) {
          // At least one interval period elapsed for the stat, so reset it. We
          // may also need to record a telemetry event for the reset.
          this.logger.info(
            `Resetting impression counter for interval ${stat.intervalSeconds}s`
          );
          this.logger.debug(
            JSON.stringify({ type, stat, elapsedMs, elapsedIntervalCount })
          );

          let newStartDateMs =
            stat.startDateMs + elapsedIntervalCount * intervalMs;

          // Compute the portion of `elapsedIntervalCount` that happened after
          // startup. This will be the interval count we report in the telemetry
          // event. By design we don't report intervals that elapsed while the
          // app wasn't running. For example, if the user stopped using Firefox
          // for a year, we don't want to report a year's worth of intervals.
          //
          // First, compute the count of intervals that elapsed before startup.
          // This is the same arithmetic used above except here it's based on
          // the startup date instead of `now`. Keep in mind that startup may be
          // before the stat's start date. Then subtract that count from
          // `elapsedIntervalCount` to get the portion after startup.
          let startupDateMs = this._getStartupDateMs();
          let elapsedIntervalCountBeforeStartup = Math.floor(
            Math.max(0, startupDateMs - stat.startDateMs) / intervalMs
          );
          let elapsedIntervalCountAfterStartup =
            elapsedIntervalCount - elapsedIntervalCountBeforeStartup;

          if (elapsedIntervalCountAfterStartup) {
            this._recordImpressionCapEvent({
              eventType: "reset",
              suggestionType: type,
              eventDateMs: newStartDateMs,
              eventCount: elapsedIntervalCountAfterStartup,
              stat: {
                ...stat,
                startDateMs:
                  stat.startDateMs +
                  elapsedIntervalCountBeforeStartup * intervalMs,
              },
            });
          }

          // Reset the stat.
          stat.startDateMs = newStartDateMs;
          stat.count = 0;
        }
      }
    }

    this.logger.debug(JSON.stringify({ newStats: this._impressionStats }));
  }

  /**
   * Records an impression cap telemetry event.
   *
   * @param {string} eventType
   *   One of: "hit", "reset"
   * @param {string} suggestionType
   *   One of: "sponsored", "nonsponsored"
   * @param {object} stat
   *   The stats object whose max count was hit or whose counter was reset.
   * @param {number} eventDateMs
   *   The `eventDate` that should be recorded in the event's `extra` object.
   *   We include this in `extra` even though events are timestamped because
   *   "reset" events are batched during periods where the user doesn't perform
   *   any searches and therefore impression counters are not reset.
   */
  _recordImpressionCapEvent({
    eventType,
    suggestionType,
    stat,
    eventCount = 1,
    eventDateMs = Date.now(),
  }) {
    // All `extra` object values must be strings.
    let extra = {
      type: suggestionType,
      eventDate: String(eventDateMs),
      eventCount: String(eventCount),
    };
    for (let [statKey, value] of Object.entries(stat)) {
      let extraKey = TELEMETRY_IMPRESSION_CAP_EXTRA_KEYS[statKey];
      if (!extraKey) {
        throw new Error("Unrecognized stats object key: " + statKey);
      }
      extra[extraKey] = String(value);
    }
    Services.telemetry.recordEvent(
      TELEMETRY_EVENT_CATEGORY,
      "impression_cap",
      eventType,
      "",
      extra
    );
  }

  /**
   * Creates a repeating timer that resets impression counters and records
   * related telemetry. Since counters are also reset when suggestions are
   * triggered, the only point of this is to make sure we record reset telemetry
   * events in a timely manner during periods when suggestions aren't triggered.
   *
   * @param {number} ms
   *   The number of milliseconds in the interval.
   */
  _setImpressionCountersResetInterval(
    ms = IMPRESSION_COUNTERS_RESET_INTERVAL_MS
  ) {
    if (this._impressionCountersResetInterval) {
      clearInterval(this._impressionCountersResetInterval);
    }
    this._impressionCountersResetInterval = setInterval(
      () => this._resetElapsedImpressionCounters(),
      ms
    );
  }

  /**
   * Gets the timestamp of app startup in ms since Unix epoch. This is only
   * defined as its own method so tests can override it to simulate arbitrary
   * startups.
   *
   * @returns {number}
   *   Startup timestamp in ms since Unix epoch.
   */
  _getStartupDateMs() {
    return Services.startup.getStartupInfo().process.getTime();
  }

  /**
   * Loads blocked suggestion digests from the pref into `_blockedDigests`.
   */
  async _loadBlockedDigests() {
    this.logger.debug(`Queueing _loadBlockedDigests`);
    await this._blockTaskQueue.queue(() => {
      this.logger.info(`Loading blocked suggestion digests`);
      let json = UrlbarPrefs.get("quicksuggest.blockedDigests");
      this.logger.debug(
        `browser.urlbar.quicksuggest.blockedDigests value: ${json}`
      );
      if (!json) {
        this.logger.info(`There are no blocked suggestion digests`);
        this._blockedDigests.clear();
      } else {
        try {
          this._blockedDigests = new Set(JSON.parse(json));
          this.logger.info(`Successfully loaded blocked suggestion digests`);
        } catch (error) {
          this.logger.error(
            `Error loading blocked suggestion digests: ${error}`
          );
        }
      }
    });
  }

  /**
   * Returns the SHA-1 digest of a string as a 40-character hex-encoded string.
   *
   * @param {string} string
   * @returns {string}
   *   The hex-encoded digest of the given string.
   */
  async _getDigest(string) {
    let stringArray = new TextEncoder().encode(string);
    let hashBuffer = await crypto.subtle.digest("SHA-1", stringArray);
    let hashArray = new Uint8Array(hashBuffer);
    return Array.from(hashArray, b => b.toString(16).padStart(2, "0")).join("");
  }

  /**
   * Updates state based on the `browser.urlbar.quicksuggest.enabled` pref.
   */
  _updateFeatureState() {
    let enabled = UrlbarPrefs.get("quickSuggestEnabled");
    if (enabled == this._quickSuggestEnabled) {
      // This method is a Nimbus `onUpdate()` callback, which means it's called
      // each time any pref is changed that is a fallback for a Nimbus variable.
      // We have many such prefs. The point of this method is to set up and tear
      // down state when quick suggest's enabled status changes, so ignore
      // updates that do not modify `quickSuggestEnabled`.
      return;
    }

    this._quickSuggestEnabled = enabled;
    this.logger.info("Updating feature state, feature enabled: " + enabled);

    Services.telemetry.setEventRecordingEnabled(
      TELEMETRY_EVENT_CATEGORY,
      enabled
    );
    if (enabled) {
      this._loadImpressionStats();
      this._loadBlockedDigests();
    }
  }

  // The most recently cached value of `UrlbarPrefs.get("quickSuggestEnabled")`.
  // The purpose of this property is only to detect changes in the feature's
  // enabled status. To determine the current status, call
  // `UrlbarPrefs.get("quickSuggestEnabled")` directly instead.
  _quickSuggestEnabled = false;

  // The result we added during the most recent query.
  _resultFromLastQuery = null;

  // An object that keeps track of impression stats per sponsored and
  // non-sponsored suggestion types. It looks like this:
  //
  //   { sponsored: statsArray, nonsponsored: statsArray }
  //
  // The `statsArray` values are arrays of stats objects, one per impression
  // cap, which look like this:
  //
  //   { intervalSeconds, startDateMs, count, maxCount, impressionDateMs }
  //
  //   {number} intervalSeconds
  //     The number of seconds in the corresponding cap's time interval.
  //   {number} startDateMs
  //     The timestamp at which the current interval period started and the
  //     object's `count` was reset to zero. This is a value returned from
  //     `Date.now()`.  When the current date/time advances past `startDateMs +
  //     1000 * intervalSeconds`, a new interval period will start and `count`
  //     will be reset to zero.
  //   {number} count
  //     The number of impressions during the current interval period.
  //   {number} maxCount
  //     The maximum number of impressions allowed during an interval period.
  //     This value is the same as the `max_count` value in the corresponding
  //     cap. It's stored in the stats object for convenience.
  //   {number} impressionDateMs
  //     The timestamp of the most recent impression, i.e., when `count` was
  //     last incremented.
  //
  // There are two types of impression caps: interval and lifetime. Interval
  // caps are periodically reset, and lifetime caps are never reset. For stats
  // objects corresponding to interval caps, `intervalSeconds` will be the
  // `interval_s` value of the cap. For stats objects corresponding to lifetime
  // caps, `intervalSeconds` will be `Infinity`.
  //
  // `_impressionStats` is kept in sync with impression caps, and there is a
  // one-to-one relationship between stats objects and caps. A stats object's
  // corresponding cap is the one with the same suggestion type (sponsored or
  // non-sponsored) and interval. See `_validateImpressionStats()` for more.
  //
  // Impression caps are stored in the remote settings config. See
  // `UrlbarQuickSuggest.confg.impression_caps`.
  _impressionStats = {};

  // Whether impression stats are currently being updated.
  _updatingImpressionStats = false;

  // Set of digests of the original URLs of blocked suggestions. A suggestion's
  // "original URL" is its URL straight from the source with an unreplaced
  // timestamp template. For details on the digests, see `_getDigest()`.
  //
  // The only reason we use URL digests is that suggestions currently do not
  // have persistent IDs. We could use the URLs themselves but SHA-1 digests are
  // only 40 chars long, so they save a little space. This is also consistent
  // with how blocked tiles on the newtab page are stored, but they use MD5. We
  // do *not* store digests for any security or obfuscation reason.
  //
  // This value is serialized as a JSON'ed array to the
  // `browser.urlbar.quicksuggest.blockedDigests` pref.
  _blockedDigests = new Set();

  // Used to serialize access to blocked suggestions. This is only necessary
  // because getting a suggestion's URL digest is async.
  _blockTaskQueue = new TaskQueue();

  // Whether blocked digests are currently being updated.
  _updatingBlockedDigests = false;

  // State related to the current Merino session.
  _merinoSessionID = null;
  _merinoSequenceNumber = 0;
  _merinoSessionTimer = null;
  _merinoSessionTimeoutMs = MERINO_SESSION_TIMEOUT_MS;
}

var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
