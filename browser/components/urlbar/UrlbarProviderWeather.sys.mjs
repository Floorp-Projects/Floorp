/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.sys.mjs",
});

const TELEMETRY_PREFIX = "contextual.services.quicksuggest";

const TELEMETRY_SCALARS = {
  BLOCK: `${TELEMETRY_PREFIX}.block_weather`,
  CLICK: `${TELEMETRY_PREFIX}.click_weather`,
  HELP: `${TELEMETRY_PREFIX}.help_weather`,
  IMPRESSION: `${TELEMETRY_PREFIX}.impression_weather`,
};

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 *
 * This provider is active only when either the Rust backend is disabled or
 * weather keywords are defined in Nimbus. When Rust is enabled and keywords are
 * not defined in Nimbus, the Rust component serves the initial weather
 * suggestion and UrlbarProviderQuickSuggest handles it along with other
 * suggestion types. Once the Rust backend is enabled by default and we no
 * longer want to experiment with weather keywords, this provider can be removed
 * along with the legacy telemetry it records.
 */
class ProviderWeather extends UrlbarProvider {
  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "Weather";
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
    this.#resultFromLastQuery = null;

    // When Rust is enabled and keywords are not defined in Nimbus, weather
    // results are created by the quick suggest provider, not this one.
    if (
      lazy.UrlbarPrefs.get("quickSuggestRustEnabled") &&
      !lazy.QuickSuggest.weather?.keywords
    ) {
      return false;
    }

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
      queryContext.isPrivate ||
      queryContext.searchMode ||
      // `QuickSuggest.weather` will be undefined if `QuickSuggest` hasn't been
      // initialized.
      !lazy.QuickSuggest.weather?.suggestion
    ) {
      return false;
    }

    let { keywords } = lazy.QuickSuggest.weather;
    if (!keywords) {
      return false;
    }

    return keywords.has(queryContext.trimmedLowerCaseSearchString);
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
    let { weather } = lazy.QuickSuggest;
    if (!weather.suggestion) {
      return;
    }

    let result = weather.makeResult(
      queryContext,
      weather.suggestion,
      queryContext.searchString
    );
    if (result) {
      result.payload.source = weather.suggestion.source;
      result.payload.provider = weather.suggestion.provider;
      addCallback(this, result);
      this.#resultFromLastQuery = result;
    }
  }

  getResultCommands(result) {
    return lazy.QuickSuggest.weather.getResultCommands(result);
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
   *
   * @param {UrlbarResult} result
   *   The result whose view will be updated.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result) {
    return lazy.QuickSuggest.weather.getViewUpdate(result);
  }

  onEngagement(state, queryContext, details, controller) {
    // Ignore engagements on other results that didn't end the session.
    if (details.result?.providerName != this.name && details.isSessionOngoing) {
      return;
    }

    // Impression and clicked telemetry are both recorded on engagement. We
    // define "impression" to mean a weather result was present in the view when
    // any result was picked.
    if (state == "engagement" && queryContext) {
      // Get the result that's visible in the view. `details.result` is the
      // engaged result, if any; if it's from this provider, then that's the
      // visible result. Otherwise fall back to #getVisibleResultFromLastQuery.
      let { result } = details;
      if (result?.providerName != this.name) {
        result = this.#getVisibleResultFromLastQuery(controller.view);
      }

      if (result) {
        this.#recordEngagementTelemetry(
          result,
          controller.input.isPrivate,
          details.result == result ? details.selType : ""
        );
      }
    }

    // Handle commands.
    if (details.result?.providerName == this.name) {
      this.#handlePossibleCommand(
        controller.view,
        details.result,
        details.selType
      );
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

    // Find a visible result.
    return view?.visibleResults?.find(r => r.providerName == this.name);
  }

  /**
   * Records engagement telemetry. This should be called only at the end of an
   * engagement when a weather result is present or when a weather result is
   * dismissed.
   *
   * @param {UrlbarResult} result
   *   The weather result that was present (and possibly picked) at the end of
   *   the engagement or that was dismissed.
   * @param {boolean} isPrivate
   *   Whether the engagement is in a private context.
   * @param {string} selType
   *   This parameter indicates the part of the row the user picked, if any, and
   *   should be one of the following values:
   *
   *   - "": The user didn't pick the row or any part of it
   *   - "weather": The user picked the main part of the row
   *   - "help": The user picked the help button
   *   - "dismiss": The user dismissed the result
   *
   *   An empty string means the user picked some other row to end the
   *   engagement, not the weather row. In that case only impression telemetry
   *   will be recorded.
   *
   *   A non-empty string means the user picked the weather row or some part of
   *   it, and both impression and click telemetry will be recorded. The
   *   non-empty-string values come from the `details.selType` passed in to
   *   `onEngagement()`; see `TelemetryEvent.typeFromElement()`.
   */
  #recordEngagementTelemetry(result, isPrivate, selType) {
    // Indexes recorded in quick suggest telemetry are 1-based, so add 1 to the
    // 0-based `result.rowIndex`.
    let telemetryResultIndex = result.rowIndex + 1;

    // impression scalars
    Services.telemetry.keyedScalarAdd(
      TELEMETRY_SCALARS.IMPRESSION,
      telemetryResultIndex,
      1
    );

    // scalars related to clicking the result and other elements in its row
    let clickScalars = [];
    let eventObject;
    switch (selType) {
      case "weather":
        clickScalars.push(TELEMETRY_SCALARS.CLICK);
        eventObject = "click";
        break;
      case "help":
        clickScalars.push(TELEMETRY_SCALARS.HELP);
        eventObject = "help";
        break;
      case "dismiss":
        clickScalars.push(TELEMETRY_SCALARS.BLOCK);
        eventObject = "block";
        break;
      default:
        if (selType) {
          eventObject = "other";
        }
        break;
    }
    for (let scalar of clickScalars) {
      Services.telemetry.keyedScalarAdd(scalar, telemetryResultIndex, 1);
    }

    // engagement event
    Services.telemetry.recordEvent(
      lazy.QuickSuggest.TELEMETRY_EVENT_CATEGORY,
      "engagement",
      eventObject || "impression_only",
      "",
      {
        match_type: "firefox-suggest",
        position: String(telemetryResultIndex),
        suggestion_type: "weather",
        source: result.payload.source,
      }
    );
  }

  #handlePossibleCommand(view, result, selType) {
    lazy.QuickSuggest.weather.handleCommand(view, result, selType);
  }

  // The result we added during the most recent query.
  #resultFromLastQuery = null;
}

export var UrlbarProviderWeather = new ProviderWeather();
