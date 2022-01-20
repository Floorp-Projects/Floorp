/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderQuickSuggest", "QUICK_SUGGEST_SOURCE"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SkippableTimer: "resource:///modules/UrlbarUtils.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["AbortController", "fetch"]);

const TIMESTAMP_TEMPLATE = "%YYYYMMDDHH%";
const TIMESTAMP_LENGTH = 10;
const TIMESTAMP_REGEXP = /^\d{10}$/;

const MERINO_ENDPOINT_PARAM_QUERY = "q";
const MERINO_ENDPOINT_PARAM_CLIENT_VARIANTS = "client_variants";
const MERINO_ENDPOINT_PARAM_PROVIDERS = "providers";

const TELEMETRY_MERINO_LATENCY = "FX_URLBAR_MERINO_LATENCY_MS";
const TELEMETRY_MERINO_RESPONSE = "FX_URLBAR_MERINO_RESPONSE";

const TELEMETRY_REMOTE_SETTINGS_LATENCY =
  "FX_URLBAR_QUICK_SUGGEST_REMOTE_SETTINGS_LATENCY_MS";

const TELEMETRY_SCALAR_IMPRESSION =
  "contextual.services.quicksuggest.impression";
const TELEMETRY_SCALAR_CLICK = "contextual.services.quicksuggest.click";
const TELEMETRY_SCALAR_HELP = "contextual.services.quicksuggest.help";

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

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
        this._fetchRemoteSettingsSuggestion(queryContext, searchString)
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
    let suggestion = allSuggestions
      .flat()
      .filter(s => s && this._canAddSuggestion(s))
      .sort((a, b) => b.score - a.score)[0];
    if (!suggestion) {
      return;
    }

    this._replaceSuggestionTemplates(suggestion);

    let payload = {
      qsSuggestion: [suggestion.full_keyword, UrlbarUtils.HIGHLIGHT.SUGGESTED],
      title: suggestion.title,
      url: suggestion.url,
      urlTimestampIndex: suggestion.urlTimestampIndex,
      icon: suggestion.icon,
      sponsoredImpressionUrl: suggestion.impression_url,
      sponsoredClickUrl: suggestion.click_url,
      sponsoredBlockId: suggestion.block_id,
      sponsoredAdvertiser: suggestion.advertiser,
      isSponsored: suggestion.is_sponsored,
      helpUrl: this.helpUrl,
      helpL10nId: "firefox-suggest-urlbar-learn-more",
      source: suggestion.source,
      requestId: suggestion.request_id,
    };

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
    );

    if (
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
        sponsoredAdvertiser,
        sponsoredImpressionUrl,
        sponsoredClickUrl,
        sponsoredBlockId,
        requestId,
      } = result.payload;
      // Always use lowercase to make the reporting consistent
      let advertiser = sponsoredAdvertiser.toLocaleLowerCase();

      let scenario = UrlbarPrefs.get("quicksuggest.scenario");
      // impression
      PartnerLinkAttribution.sendContextualServicesPing(
        {
          scenario,
          advertiser,
          block_id: sponsoredBlockId,
          position: telemetryResultIndex,
          reporting_url: sponsoredImpressionUrl,
          is_clicked: isQuickSuggestLinkClicked,
          request_id: requestId,
        },
        CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION
      );
      // click
      if (isQuickSuggestLinkClicked) {
        PartnerLinkAttribution.sendContextualServicesPing(
          {
            scenario,
            advertiser,
            block_id: sponsoredBlockId,
            position: telemetryResultIndex,
            reporting_url: sponsoredClickUrl,
            request_id: requestId,
          },
          CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION
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
   * Fetches a remote settings suggestion.
   *
   * @param {UrlbarQueryContext} queryContext
   * @param {string} searchString
   * @returns {object}
   *   The remote settings suggestion or null if there's no match.
   */
  async _fetchRemoteSettingsSuggestion(queryContext, searchString) {
    let instance = this.queryInstance;

    let suggestion;
    TelemetryStopwatch.start(TELEMETRY_REMOTE_SETTINGS_LATENCY, queryContext);
    try {
      suggestion = await UrlbarQuickSuggest.query(searchString);
      TelemetryStopwatch.finish(
        TELEMETRY_REMOTE_SETTINGS_LATENCY,
        queryContext
      );
      if (instance != this.queryInstance) {
        return null;
      }
    } catch (error) {
      TelemetryStopwatch.cancel(
        TELEMETRY_REMOTE_SETTINGS_LATENCY,
        queryContext
      );
      this.logger.error("Could not fetch remote settings suggestion: " + error);
    }

    return suggestion;
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
    url.searchParams.set(MERINO_ENDPOINT_PARAM_QUERY, searchString);

    let clientVariants = UrlbarPrefs.get("merino.clientVariants");
    if (clientVariants) {
      url.searchParams.set(
        MERINO_ENDPOINT_PARAM_CLIENT_VARIANTS,
        clientVariants
      );
    }

    let providers = UrlbarPrefs.get("merino.providers");
    if (providers) {
      url.searchParams.set(MERINO_ENDPOINT_PARAM_PROVIDERS, providers);
    } else if (
      !UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") &&
      !UrlbarPrefs.get("suggest.quicksuggest.sponsored")
    ) {
      // Data collection is enabled but suggestions are not. Set the `providers`
      // param to an empty string to tell Merino not to fetch any suggestions.
      url.searchParams.set(MERINO_ENDPOINT_PARAM_PROVIDERS, "");
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
   * Returns whether a given suggestion can be added for a query, assuming the
   * provider itself should be active.
   *
   * @param {object} suggestion
   *   A suggestion object fetched from UrlbarQuickSuggest.
   * @returns {boolean}
   *   Whether the suggestion can be added.
   */
  _canAddSuggestion(suggestion) {
    return (
      (suggestion.is_sponsored &&
        UrlbarPrefs.get("suggest.quicksuggest.sponsored")) ||
      (!suggestion.is_sponsored &&
        UrlbarPrefs.get("suggest.quicksuggest.nonsponsored"))
    );
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
