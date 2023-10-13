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
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

const WEATHER_PROVIDER_DISPLAY_NAME = "AccuWeather";

const TELEMETRY_PREFIX = "contextual.services.quicksuggest";

const TELEMETRY_SCALARS = {
  BLOCK: `${TELEMETRY_PREFIX}.block_weather`,
  CLICK: `${TELEMETRY_PREFIX}.click_weather`,
  HELP: `${TELEMETRY_PREFIX}.help_weather`,
  IMPRESSION: `${TELEMETRY_PREFIX}.impression_weather`,
};

const RESULT_MENU_COMMAND = {
  HELP: "help",
  INACCURATE_LOCATION: "inaccurate_location",
  NOT_INTERESTED: "not_interested",
  NOT_RELEVANT: "not_relevant",
  SHOW_LESS_FREQUENTLY: "show_less_frequently",
};

const WEATHER_DYNAMIC_TYPE = "weather";
const WEATHER_VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "currentConditions",
      tag: "span",
      children: [
        {
          name: "currently",
          tag: "div",
        },
        {
          name: "currentTemperature",
          tag: "div",
          children: [
            {
              name: "temperature",
              tag: "span",
            },
            {
              name: "weatherIcon",
              tag: "img",
            },
          ],
        },
      ],
    },
    {
      name: "summary",
      tag: "span",
      overflowable: true,
      children: [
        {
          name: "top",
          tag: "div",
          children: [
            {
              name: "topNoWrap",
              tag: "span",
              children: [
                { name: "title", tag: "span", classList: ["urlbarView-title"] },
                {
                  name: "titleSeparator",
                  tag: "span",
                  classList: ["urlbarView-title-separator"],
                },
              ],
            },
            {
              name: "url",
              tag: "span",
              classList: ["urlbarView-url"],
            },
          ],
        },
        {
          name: "middle",
          tag: "div",
          children: [
            {
              name: "middleNoWrap",
              tag: "span",
              overflowable: true,
              children: [
                {
                  name: "summaryText",
                  tag: "span",
                },
                {
                  name: "summaryTextSeparator",
                  tag: "span",
                },
                {
                  name: "highLow",
                  tag: "span",
                },
              ],
            },
            {
              name: "highLowWrap",
              tag: "span",
            },
          ],
        },
        {
          name: "bottom",
          tag: "div",
        },
      ],
    },
  ],
};

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderWeather extends UrlbarProvider {
  constructor(...args) {
    super(...args);
    lazy.UrlbarResult.addDynamicResultType(WEATHER_DYNAMIC_TYPE);
    lazy.UrlbarView.addDynamicViewTemplate(
      WEATHER_DYNAMIC_TYPE,
      WEATHER_VIEW_TEMPLATE
    );
  }

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

    return keywords.has(queryContext.searchString.trim().toLocaleLowerCase());
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
    let { suggestion } = lazy.QuickSuggest.weather;
    if (!suggestion) {
      return;
    }

    let unit = Services.locale.regionalPrefsLocales[0] == "en-US" ? "f" : "c";
    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        url: suggestion.url,
        iconId: suggestion.current_conditions.icon_id,
        helpUrl: lazy.QuickSuggest.HELP_URL,
        requestId: suggestion.request_id,
        source: suggestion.source,
        provider: suggestion.provider,
        dynamicType: WEATHER_DYNAMIC_TYPE,
        city: suggestion.city_name,
        temperatureUnit: unit,
        temperature: suggestion.current_conditions.temperature[unit],
        currentConditions: suggestion.current_conditions.summary,
        forecast: suggestion.forecast.summary,
        high: suggestion.forecast.high[unit],
        low: suggestion.forecast.low[unit],
        shouldNavigate: true,
      }
    );

    result.showFeedbackMenu = true;
    result.suggestedIndex = queryContext.searchString ? 1 : 0;

    addCallback(this, result);
    this.#resultFromLastQuery = result;
  }

  getResultCommands(result) {
    let commands = [
      {
        name: RESULT_MENU_COMMAND.INACCURATE_LOCATION,
        l10n: {
          id: "firefox-suggest-weather-command-inaccurate-location",
        },
      },
    ];

    if (lazy.QuickSuggest.weather.canIncrementMinKeywordLength) {
      commands.push({
        name: RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY,
        l10n: {
          id: "firefox-suggest-command-show-less-frequently",
        },
      });
    }

    commands.push(
      {
        l10n: {
          id: "firefox-suggest-command-dont-show-this",
        },
        children: [
          {
            name: RESULT_MENU_COMMAND.NOT_RELEVANT,
            l10n: {
              id: "firefox-suggest-command-not-relevant",
            },
          },
          {
            name: RESULT_MENU_COMMAND.NOT_INTERESTED,
            l10n: {
              id: "firefox-suggest-command-not-interested",
            },
          },
        ],
      },
      { name: "separator" },
      {
        name: RESULT_MENU_COMMAND.HELP,
        l10n: {
          id: "urlbar-result-menu-learn-more-about-firefox-suggest",
        },
      }
    );

    return commands;
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
   *
   * @param {UrlbarResult} result
   *   The result whose view will be updated.
   * @param {Map} idsByName
   *   A Map from an element's name, as defined by the provider; to its ID in
   *   the DOM, as defined by the browser.This is useful if parts of the view
   *   update depend on element IDs, as some ARIA attributes do.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result, idsByName) {
    let uppercaseUnit = result.payload.temperatureUnit.toUpperCase();

    return {
      currently: {
        l10n: {
          id: "firefox-suggest-weather-currently",
          cacheable: true,
        },
      },
      temperature: {
        l10n: {
          id: "firefox-suggest-weather-temperature",
          args: {
            value: result.payload.temperature,
            unit: uppercaseUnit,
          },
          cacheable: true,
          excludeArgsFromCacheKey: true,
        },
      },
      weatherIcon: {
        attributes: { iconId: result.payload.iconId },
      },
      title: {
        l10n: {
          id: "firefox-suggest-weather-title",
          args: { city: result.payload.city },
          cacheable: true,
          excludeArgsFromCacheKey: true,
        },
      },
      url: {
        textContent: result.payload.url,
      },
      summaryText: {
        l10n: {
          id: "firefox-suggest-weather-summary-text",
          args: {
            currentConditions: result.payload.currentConditions,
            forecast: result.payload.forecast,
          },
          cacheable: true,
          excludeArgsFromCacheKey: true,
        },
      },
      highLow: {
        l10n: {
          id: "firefox-suggest-weather-high-low",
          args: {
            high: result.payload.high,
            low: result.payload.low,
            unit: uppercaseUnit,
          },
          cacheable: true,
          excludeArgsFromCacheKey: true,
        },
      },
      highLowWrap: {
        l10n: {
          id: "firefox-suggest-weather-high-low",
          args: {
            high: result.payload.high,
            low: result.payload.low,
            unit: uppercaseUnit,
          },
        },
      },
      bottom: {
        l10n: {
          id: "firefox-suggest-weather-sponsored",
          args: { provider: WEATHER_PROVIDER_DISPLAY_NAME },
          cacheable: true,
        },
      },
    };
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
    switch (selType) {
      case RESULT_MENU_COMMAND.HELP:
        // "help" is handled by UrlbarInput, no need to do anything here.
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        this.logger.info("Dismissing weather result");
        lazy.UrlbarPrefs.set("suggest.weather", false);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-all",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.INACCURATE_LOCATION:
        // Currently the only way we record this feedback is in the Glean
        // engagement event. As with all commands, it will be recorded with an
        // `engagement_type` value that is the command's name, in this case
        // `inaccurate_location`.
        view.acknowledgeFeedback(result);
        break;
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        view.acknowledgeFeedback(result);
        lazy.QuickSuggest.weather.incrementMinKeywordLength();
        if (!lazy.QuickSuggest.weather.canIncrementMinKeywordLength) {
          view.invalidateResultMenuCommands();
        }
        break;
    }
  }

  // The result we added during the most recent query.
  #resultFromLastQuery = null;
}

export var UrlbarProviderWeather = new ProviderWeather();
