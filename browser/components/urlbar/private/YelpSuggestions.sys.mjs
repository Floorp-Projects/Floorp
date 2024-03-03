/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const RESULT_MENU_COMMAND = {
  HELP: "help",
  INACCURATE_LOCATION: "inaccurate_location",
  NOT_INTERESTED: "not_interested",
  NOT_RELEVANT: "not_relevant",
  SHOW_LESS_FREQUENTLY: "show_less_frequently",
};

/**
 * A feature for Yelp suggestions.
 */
export class YelpSuggestions extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") &&
      lazy.UrlbarPrefs.get("yelpFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.yelp")
    );
  }

  get enablingPreferences() {
    return ["suggest.quicksuggest.sponsored", "suggest.yelp"];
  }

  get rustSuggestionTypes() {
    return ["Yelp"];
  }

  get showLessFrequentlyCount() {
    const count = lazy.UrlbarPrefs.get("yelp.showLessFrequentlyCount") || 0;
    return Math.max(count, 0);
  }

  get canShowLessFrequently() {
    const cap =
      lazy.UrlbarPrefs.get("yelpShowLessFrequentlyCap") ||
      lazy.QuickSuggest.backend.config?.showLessFrequentlyCap ||
      0;
    return !cap || this.showLessFrequentlyCount < cap;
  }

  getSuggestionTelemetryType() {
    return "yelp";
  }

  enable(enabled) {
    if (!enabled) {
      this.#merino = null;
    }
  }

  async makeResult(queryContext, suggestion, searchString) {
    // If the user clicked "Show less frequently" at least once or if the
    // subject wasn't typed in full, then apply the min length threshold and
    // return null if the entire search string is too short.
    if (
      (this.showLessFrequentlyCount || !suggestion.subjectExactMatch) &&
      searchString.length < this.#minKeywordLength
    ) {
      return null;
    }

    suggestion.is_top_pick = lazy.UrlbarPrefs.get("yelpSuggestPriority");

    let url = new URL(suggestion.url);
    let title = suggestion.title;
    if (!url.searchParams.has(suggestion.locationParam)) {
      let city = await this.#fetchCity();

      // If we can't get city from Merino, rely on Yelp own.
      if (city) {
        url.searchParams.set(suggestion.locationParam, city);

        if (!suggestion.hasLocationSign) {
          title += " in";
        }

        title += ` ${city}`;
      }
    }

    url.searchParams.set("utm_medium", "partner");
    url.searchParams.set("utm_source", "mozilla");

    let resultProperties = {
      isRichSuggestion: true,
      richSuggestionIconSize: 38,
      showFeedbackMenu: true,
    };
    if (!suggestion.is_top_pick) {
      resultProperties.richSuggestionIconSize = 16;
      resultProperties.isSuggestedIndexRelativeToGroup = true;
      resultProperties.suggestedIndex = lazy.UrlbarPrefs.get(
        "yelpSuggestNonPriorityIndex"
      );
    }

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          url: url.toString(),
          originalUrl: suggestion.url,
          title: [title, lazy.UrlbarUtils.HIGHLIGHT.TYPED],
          shouldShowUrl: true,
          bottomTextL10n: { id: "firefox-suggest-yelp-bottom-text" },
        })
      ),
      resultProperties
    );
  }

  getResultCommands() {
    let commands = [
      {
        name: RESULT_MENU_COMMAND.INACCURATE_LOCATION,
        l10n: {
          id: "firefox-suggest-weather-command-inaccurate-location",
        },
      },
    ];

    if (this.canShowLessFrequently) {
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

  handleCommand(view, result, selType, searchString) {
    switch (selType) {
      case RESULT_MENU_COMMAND.HELP:
        // "help" is handled by UrlbarInput, no need to do anything here.
        break;
      case RESULT_MENU_COMMAND.INACCURATE_LOCATION:
        // Currently the only way we record this feedback is in the Glean
        // engagement event. As with all commands, it will be recorded with an
        // `engagement_type` value that is the command's name, in this case
        // `inaccurate_location`.
        view.acknowledgeFeedback(result);
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-one-yelp",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
        lazy.UrlbarPrefs.set("suggest.yelp", false);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-all-yelp",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        view.acknowledgeFeedback(result);
        this.incrementShowLessFrequentlyCount();
        if (!this.canShowLessFrequently) {
          view.invalidateResultMenuCommands();
        }
        lazy.UrlbarPrefs.set("yelp.minKeywordLength", searchString.length + 1);
        break;
    }
  }

  incrementShowLessFrequentlyCount() {
    if (this.canShowLessFrequently) {
      lazy.UrlbarPrefs.set(
        "yelp.showLessFrequentlyCount",
        this.showLessFrequentlyCount + 1
      );
    }
  }

  get #minKeywordLength() {
    // It's unusual to get both a Nimbus variable and its fallback pref at the
    // same time, but we have a good reason. To recap, if a variable doesn't
    // have a value, then the value of its fallback will be returned; otherwise
    // the variable value will be returned. That's usually what we want, but for
    // Yelp, we set the pref each time the user clicks "show less frequently",
    // and we want the variable to act only as an initial min length. In other
    // words, if the pref has a user value (because we set it), use it;
    // otherwise use the initial value defined by the variable.
    return Math.max(
      lazy.UrlbarPrefs.get("yelpMinKeywordLength") || 0,
      lazy.UrlbarPrefs.get("yelp.minKeywordLength") || 0,
      0
    );
  }

  async #fetchCity() {
    if (!this.#merino) {
      this.#merino = new lazy.MerinoClient(this.constructor.name);
    }

    let results = await this.#merino.fetch({
      providers: ["geolocation"],
      query: "",
    });

    if (!results.length) {
      return null;
    }

    let { city, region } = results[0].custom_details.geolocation;
    return [city, region].filter(loc => !!loc).join(", ");
  }

  #merino = null;
}
