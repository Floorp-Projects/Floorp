/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderQuickSuggest"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["AbortController", "fetch"]);

const MERINO_ENDPOINT_PARAM_QUERY = "q";

const TELEMETRY_MERINO_LATENCY = "FX_URLBAR_MERINO_LATENCY_MS";
const TELEMETRY_SCALAR_IMPRESSION =
  "contextual.services.quicksuggest.impression";
const TELEMETRY_SCALAR_CLICK = "contextual.services.quicksuggest.click";
const TELEMETRY_SCALAR_HELP = "contextual.services.quicksuggest.help";

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderQuickSuggest extends UrlbarProvider {
  constructor(...args) {
    super(...args);
    this._updateExperimentState();
    UrlbarPrefs.addObserver(this);
    NimbusFeatures.urlbar.onUpdate(this._updateExperimentState);
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
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    this._addedResultInLastQuery = false;

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
      UrlbarPrefs.get("suggest.quicksuggest")
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

    // We currently have two sources for quick suggest: remote settings
    // (from `UrlbarQuickSuggest`) and Merino.
    let promises = [];
    if (UrlbarPrefs.get("quickSuggestRemoteSettingsEnabled")) {
      promises.push(UrlbarQuickSuggest.query(searchString));
    }
    if (UrlbarPrefs.get("merinoEnabled") && queryContext.allowRemoteResults()) {
      promises.push(this._fetchMerinoSuggestions(queryContext, searchString));
    }

    let allSuggestions = await Promise.all(promises);
    if (instance != this.queryInstance) {
      return;
    }

    // Filter out sponsored suggestions if they're disabled. Also filter out
    // null suggestions since both the remote settings and Merino fetches return
    // null when there are no matches. Take the remaining suggestion with the
    // largest score.
    let suggestion = allSuggestions
      .flat()
      .filter(
        s =>
          s &&
          (!s.is_sponsored || UrlbarPrefs.get("suggest.quicksuggest.sponsored"))
      )
      .sort((a, b) => b.score - a.score)[0];
    if (!suggestion) {
      return;
    }

    let payload = {
      qsSuggestion: [suggestion.full_keyword, UrlbarUtils.HIGHLIGHT.SUGGESTED],
      title: suggestion.title,
      url: suggestion.url,
      icon: suggestion.icon,
      sponsoredImpressionUrl: suggestion.impression_url,
      sponsoredClickUrl: suggestion.click_url,
      sponsoredBlockId: suggestion.block_id,
      sponsoredAdvertiser: suggestion.advertiser,
      isSponsored: suggestion.is_sponsored,
      helpUrl: this.helpUrl,
      helpL10nId: "firefox-suggest-urlbar-learn-more",
    };

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
    );
    result.isSuggestedIndexRelativeToGroup = true;
    result.suggestedIndex = UrlbarPrefs.get(
      suggestion.is_sponsored
        ? "quickSuggestSponsoredIndex"
        : "quickSuggestNonSponsoredIndex"
    );
    addCallback(this, result);

    this._addedResultInLastQuery = true;

    // Record the Nimbus "exposure" event. Note that `recordExposureEvent` will
    // make sure only one event gets recorded even it is called multiple times
    // in the same browser session. However, it's an expensive call regardless,
    // so do it only once per browser session and do it on idle.
    if (!this._recordedExposureEvent) {
      this._recordedExposureEvent = true;
      Services.tm.idleDispatchToMainThread(() =>
        NimbusFeatures.urlbar.recordExposureEvent({ once: true })
      );
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
    if (!this._addedResultInLastQuery) {
      return;
    }
    this._addedResultInLastQuery = false;

    // Per spec, we update telemetry only when the user picks a result, i.e.,
    // when `state` is "engagement".
    if (state != "engagement") {
      return;
    }

    // Get the index of the quick suggest result. Usually it will be last, so to
    // avoid an O(n) lookup in the common case, check the last result first. It
    // may not be last if `browser.urlbar.showSearchSuggestionsFirst` is false
    // or its position is configured differently via Nimbus.
    let resultIndex = queryContext.results.length - 1;
    let result = queryContext.results[resultIndex];
    if (result.providerName != this.name) {
      resultIndex = queryContext.results.findIndex(
        r => r.providerName == this.name
      );
      if (resultIndex < 0) {
        this.logger.error(`Could not find quick suggest result`);
        return;
      }
      result = queryContext.results[resultIndex];
    }

    // Record telemetry.  We want to record the 1-based index of the result, so
    // add 1 to the 0-based resultIndex.
    let telemetryResultIndex = resultIndex + 1;

    // impression scalar
    Services.telemetry.keyedScalarAdd(
      TELEMETRY_SCALAR_IMPRESSION,
      telemetryResultIndex,
      1
    );

    if (details.selIndex == resultIndex) {
      // click or help scalar
      Services.telemetry.keyedScalarAdd(
        details.selType == "help"
          ? TELEMETRY_SCALAR_HELP
          : TELEMETRY_SCALAR_CLICK,
        telemetryResultIndex,
        1
      );
    }

    // Send the custom impression and click pings
    if (!isPrivate) {
      let isQuickSuggestLinkClicked =
        details.selIndex == resultIndex && details.selType !== "help";
      let {
        qsSuggestion, // The full keyword
        sponsoredAdvertiser,
        sponsoredImpressionUrl,
        sponsoredClickUrl,
        sponsoredBlockId,
      } = result.payload;

      let searchQuery = "";
      let matchedKeywords = "";
      let scenario = UrlbarPrefs.get("quicksuggest.scenario");
      // Only collect the search query and matched keywords for "online" scenario.
      // For other scenarios, those fields are set as empty strings.
      if (scenario === "online") {
        matchedKeywords = qsSuggestion || details.searchString;
        searchQuery = details.searchString;
      }

      // impression
      PartnerLinkAttribution.sendContextualServicesPing(
        {
          scenario,
          search_query: searchQuery,
          matched_keywords: matchedKeywords,
          advertiser: sponsoredAdvertiser,
          block_id: sponsoredBlockId,
          position: telemetryResultIndex,
          reporting_url: sponsoredImpressionUrl,
          is_clicked: isQuickSuggestLinkClicked,
        },
        CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION
      );
      // click
      if (isQuickSuggestLinkClicked) {
        PartnerLinkAttribution.sendContextualServicesPing(
          {
            scenario,
            advertiser: sponsoredAdvertiser,
            block_id: sponsoredBlockId,
            position: telemetryResultIndex,
            reporting_url: sponsoredClickUrl,
          },
          CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION
        );
      }
    }
  }

  /**
   * Called when a urlbar pref changes.  We use this to listen for changes to
   * `browser.urlbar.suggest.quicksuggest` so we can record a telemetry event.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "suggest.quicksuggest":
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
    try {
      this._merinoFetchController?.abort();
    } catch (error) {
      this.logger.error(error);
    }
    this._merinoFetchController = null;
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

    // Fetch a response from the endpoint.
    let response;
    let controller;
    try {
      let url = new URL(UrlbarPrefs.get("merino.endpointURL"));
      url.searchParams.set(MERINO_ENDPOINT_PARAM_QUERY, searchString);

      controller = this._merinoFetchController = new AbortController();
      TelemetryStopwatch.start(TELEMETRY_MERINO_LATENCY, queryContext);
      response = await fetch(url, {
        signal: controller.signal,
      });
      TelemetryStopwatch.finish(TELEMETRY_MERINO_LATENCY, queryContext);
      if (instance != this.queryInstance) {
        return null;
      }
    } catch (error) {
      TelemetryStopwatch.cancel(TELEMETRY_MERINO_LATENCY, queryContext);
      if (error.name != "AbortError") {
        this.logger.error(error);
      }
    } finally {
      if (controller == this._merinoFetchController) {
        this._merinoFetchController = null;
      }
    }

    if (!response) {
      return null;
    }

    // Get the response body as an object.
    let body;
    try {
      body = await response.json();
      if (instance != this.queryInstance) {
        return null;
      }
    } catch (error) {
      this.logger.error(error);
    }

    if (!body?.suggestions?.length) {
      return null;
    }

    let { suggestions } = body;
    if (!Array.isArray(suggestions)) {
      this.logger.error("Unexpected Merino response: " + JSON.stringify(body));
      return null;
    }

    return suggestions;
  }

  /**
   * Updates state based on the `browser.urlbar.quicksuggest.enabled` pref.
   * Enable/disable event telemetry and ensure QuickSuggest module is loaded
   * when enabled.
   */
  _updateExperimentState() {
    Services.telemetry.setEventRecordingEnabled(
      TELEMETRY_EVENT_CATEGORY,
      UrlbarPrefs.get("quickSuggestEnabled")
    );

    // QuickSuggest is only loaded by the UrlBar on it's first query, however
    // there is work it can preload when idle instead of starting it on user
    // input. Referencing it here will trigger its import and init.
    if (UrlbarPrefs.get("quickSuggestEnabled")) {
      UrlbarQuickSuggest; // eslint-disable-line no-unused-expressions
    }
  }

  // Whether we added a result during the most recent query.
  _addedResultInLastQuery = false;
}

var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
