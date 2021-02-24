/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderQuickSuggest"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

const ONBOARDING_COUNT_PREF = "quicksuggest.onboardingCount";
const ONBOARDING_MAX_COUNT_PREF = "quicksuggest.onboardingMaxCount";

// TODO (bug 1693671): Replace this URL with the final URL of the blog post.
const ONBOARDING_URL = "https://mozilla.org/";
const ONBOARDING_TEXT = "Learn more about Firefox Suggests";

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderQuickSuggest extends UrlbarProvider {
  // Whether we added a result during the most recent query.
  _addedResultInLastQuery = false;

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
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
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
      UrlbarPrefs.get("quicksuggest.enabled") &&
      UrlbarPrefs.get("suggest.quicksuggest") &&
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
    let suggestion = await UrlbarQuickSuggest.query(queryContext.searchString);
    if (!suggestion || instance != this.queryInstance) {
      return;
    }

    let payload = {
      title: suggestion.title,
      url: suggestion.url,
      icon: suggestion.icon,
      isSponsored: true,
    };

    // Show the help button if we haven't reached the max onboarding count yet.
    if (this._onboardingCount < this._onboardingMaxCount) {
      payload.helpUrl = ONBOARDING_URL;
      payload.helpTitle = ONBOARDING_TEXT;
    }

    let result = new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
      payload
    );
    result.suggestedIndex = UrlbarPrefs.get("quicksuggest.suggestedIndex");
    if (result.suggestedIndex == -1) {
      result.suggestedIndex = UrlbarPrefs.get("maxRichResults") - 1;
    }
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
    if (
      state == "engagement" &&
      this._addedResultInLastQuery &&
      this._onboardingCount < this._onboardingMaxCount
    ) {
      this._onboardingCount++;
    }
    this._addedResultInLastQuery = false;
  }

  get _onboardingCount() {
    return UrlbarPrefs.get(ONBOARDING_COUNT_PREF);
  }

  set _onboardingCount(value) {
    UrlbarPrefs.set(ONBOARDING_COUNT_PREF, value);
  }

  get _onboardingMaxCount() {
    return UrlbarPrefs.get(ONBOARDING_MAX_COUNT_PREF);
  }
}

var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
