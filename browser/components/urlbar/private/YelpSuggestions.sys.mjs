/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const RESULT_MENU_COMMAND = {
  HELP: "help",
  INACCURATE_LOCATION: "inaccurate_location",
  NOT_INTERESTED: "not_interested",
  SHOW_LESS_FREQUENTLY: "show_less_frequentry",
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
    return ["suggest.yelp"];
  }

  get rustSuggestionTypes() {
    return ["Yelp"];
  }

  getSuggestionTelemetryType(suggestion) {
    return "yelp";
  }

  makeResult(queryContext, suggestion, searchString) {
    if (
      this.#showLessFrequentlyCount &&
      searchString.length <=
        this.#showLessFrequentlyCount + this.#minKeywordLength
    ) {
      return null;
    }

    suggestion.is_top_pick = lazy.UrlbarPrefs.get("yelpSuggestPriority");

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          // TODO: Should define Yelp icon here. Bug 1874624.
          url: suggestion.url,
          title: suggestion.title,
          shouldShowUrl: true,
          bottomTextL10n: { id: "firefox-suggest-yelp-bottom-text" },
        })
      ),
      {
        richSuggestionIconSize: 24,
      }
    );
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

    if (this.#canShowLessFrequently) {
      commands.push({
        name: RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY,
        l10n: {
          id: "firefox-suggest-command-show-less-frequently",
        },
      });
    }

    commands.push(
      {
        name: RESULT_MENU_COMMAND.NOT_INTERESTED,
        l10n: {
          id: "firefox-suggest-command-dont-show-this",
        },
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

  handleCommand(view, result, selType) {
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
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
        lazy.UrlbarPrefs.set("suggest.yelp", false);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-all",
        };
        view.controller.removeResult(result);
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        view.acknowledgeFeedback(result);
        this.#incrementShowLessFrequentlyCount();
        if (!this.#canShowLessFrequently) {
          view.invalidateResultMenuCommands();
        }
        break;
    }
  }

  #incrementShowLessFrequentlyCount() {
    if (this.#canShowLessFrequently) {
      lazy.UrlbarPrefs.set(
        "yelp.showLessFrequentlyCount",
        this.#showLessFrequentlyCount + 1
      );
    }
  }

  get #minKeywordLength() {
    const len = lazy.UrlbarPrefs.get("yelpMinKeywordLength") || 0;
    return Math.max(len, 0);
  }

  get #showLessFrequentlyCount() {
    const count = lazy.UrlbarPrefs.get("yelp.showLessFrequentlyCount") || 0;
    return Math.max(count, 0);
  }

  get #canShowLessFrequently() {
    const cap = lazy.UrlbarPrefs.get("yelpShowLessFrequentlyCap") || 0;
    return !cap || this.#showLessFrequentlyCount < cap;
  }
}
