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

// These prefs are relative to the `browser.urlbar` branch.
const SUGGEST_PREF = "suggest.quicksuggest";

const FEATURE_NAME = "Firefox Suggest";
const NONSPONSORED_ACTION_TEXT = FEATURE_NAME;
const HELP_TITLE = `Learn more about ${FEATURE_NAME}`;

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
   * @returns {string} The name of the Firefox Suggest feature, suitable for
   *   display to the user. en-US only for now.
   */
  get featureName() {
    return FEATURE_NAME;
  }

  /**
   * @returns {string} The help URL for the Quick Suggest feature.
   */
  get helpUrl() {
    return (
      this._helpUrl ||
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
      UrlbarPrefs.get("quickSuggestEnabled") &&
      (UrlbarPrefs.get("quicksuggest.showedOnboardingDialog") ||
        !UrlbarPrefs.get("quickSuggestShouldShowOnboardingDialog")) &&
      UrlbarPrefs.get(SUGGEST_PREF) &&
      UrlbarPrefs.get("suggest.searches") &&
      UrlbarPrefs.get("browser.search.suggest.enabled") &&
      (!queryContext.isPrivate ||
        UrlbarPrefs.get("browser.search.suggest.enabled.private"))
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
    let suggestion = await UrlbarQuickSuggest.query(
      queryContext.searchString.trimStart()
    );
    if (!suggestion || instance != this.queryInstance) {
      return;
    }

    let payload = {
      qsSuggestion: [suggestion.fullKeyword, UrlbarUtils.HIGHLIGHT.SUGGESTED],
      title: suggestion.title,
      url: suggestion.url,
      icon: suggestion.icon,
      sponsoredImpressionUrl: suggestion.impression_url,
      sponsoredClickUrl: suggestion.click_url,
      sponsoredBlockId: suggestion.block_id,
      sponsoredAdvertiser: suggestion.advertiser,
      isSponsored: true,
      helpUrl: this.helpUrl,
      helpTitle: HELP_TITLE,
    };

    if (!suggestion.isSponsored) {
      payload.sponsoredText = NONSPONSORED_ACTION_TEXT;
    }

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, payload)
    );
    addCallback(this, result);

    this._addedResultInLastQuery = true;
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

    // Get the index of the quick suggest result.
    let resultIndex = queryContext.results.length - 1;
    let lastResult = queryContext.results[resultIndex];
    if (!lastResult?.payload.isSponsored) {
      Cu.reportError(`Last result is not a quick suggest`);
      return;
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
      } = lastResult.payload;
      // impression
      PartnerLinkAttribution.sendContextualServicesPing(
        {
          search_query: details.searchString,
          matched_keywords: details.searchString,
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
      case SUGGEST_PREF:
        Services.telemetry.recordEvent(
          TELEMETRY_EVENT_CATEGORY,
          "enable_toggled",
          UrlbarPrefs.get(SUGGEST_PREF) ? "enabled" : "disabled"
        );
        break;
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
      NimbusFeatures.urlbar.getValue().quickSuggestEnabled
    );
    // QuickSuggest is only loaded by the UrlBar on it's first query, however
    // there is work it can preload when idle instead of starting it on user
    // input. Referencing it here will trigger its import and init.
    if (NimbusFeatures.urlbar.getValue().quickSuggestEnabled) {
      UrlbarQuickSuggest; // eslint-disable-line no-unused-expressions
    }
  }

  // Whether we added a result during the most recent query.
  _addedResultInLastQuery = false;

  // This is intended for tests and allows them to set a different help URL.
  _helpUrl = undefined;
}

var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
