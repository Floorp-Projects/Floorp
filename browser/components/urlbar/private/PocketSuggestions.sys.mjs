/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  SuggestionsMap: "resource:///modules/urlbar/private/SuggestBackendJs.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const RESULT_MENU_COMMAND = {
  HELP: "help",
  NOT_INTERESTED: "not_interested",
  NOT_RELEVANT: "not_relevant",
  SHOW_LESS_FREQUENTLY: "show_less_frequently",
};

/**
 * A feature that manages Pocket suggestions in remote settings.
 */
export class PocketSuggestions extends BaseFeature {
  constructor() {
    super();
    this.#lowConfidenceSuggestionsMap = new lazy.SuggestionsMap();
    this.#highConfidenceSuggestionsMap = new lazy.SuggestionsMap();
  }

  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("pocketFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.pocket") &&
      lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored")
    );
  }

  get enablingPreferences() {
    return ["suggest.pocket", "suggest.quicksuggest.nonsponsored"];
  }

  get merinoProvider() {
    return "pocket";
  }

  get rustSuggestionTypes() {
    return ["Pocket"];
  }

  get showLessFrequentlyCount() {
    let count = lazy.UrlbarPrefs.get("pocket.showLessFrequentlyCount") || 0;
    return Math.max(count, 0);
  }

  get canShowLessFrequently() {
    let cap =
      lazy.UrlbarPrefs.get("pocketShowLessFrequentlyCap") ||
      lazy.QuickSuggest.jsBackend.config.show_less_frequently_cap ||
      0;
    return !cap || this.showLessFrequentlyCount < cap;
  }

  enable(enabled) {
    if (enabled) {
      lazy.QuickSuggest.jsBackend.register(this);
    } else {
      lazy.QuickSuggest.jsBackend.unregister(this);
      this.#lowConfidenceSuggestionsMap.clear();
      this.#highConfidenceSuggestionsMap.clear();
    }
  }

  async queryRemoteSettings(searchString) {
    // If the search string matches high confidence suggestions, they should be
    // treated as top picks. Otherwise try to match low confidence suggestions.
    let is_top_pick = false;
    let suggestions = this.#highConfidenceSuggestionsMap.get(searchString);
    if (suggestions.length) {
      is_top_pick = true;
    } else {
      suggestions = this.#lowConfidenceSuggestionsMap.get(searchString);
    }

    let lowerSearchString = searchString.toLocaleLowerCase();
    return suggestions.map(suggestion => {
      // Add `full_keyword` to each matched suggestion. It should be the longest
      // keyword that starts with the user's search string.
      let full_keyword = lowerSearchString;
      let keywords = is_top_pick
        ? suggestion.highConfidenceKeywords
        : suggestion.lowConfidenceKeywords;
      for (let keyword of keywords) {
        if (
          keyword.startsWith(lowerSearchString) &&
          full_keyword.length < keyword.length
        ) {
          full_keyword = keyword;
        }
      }
      return { ...suggestion, is_top_pick, full_keyword };
    });
  }

  async onRemoteSettingsSync(rs) {
    let records = await rs.get({ filters: { type: "pocket-suggestions" } });
    if (!this.isEnabled) {
      return;
    }

    let lowMap = new lazy.SuggestionsMap();
    let highMap = new lazy.SuggestionsMap();

    this.logger.debug(`Got ${records.length} records`);
    for (let record of records) {
      let { buffer } = await rs.attachments.download(record);
      if (!this.isEnabled) {
        return;
      }

      let suggestions = JSON.parse(new TextDecoder("utf-8").decode(buffer));
      this.logger.debug(`Adding ${suggestions.length} suggestions`);

      await lowMap.add(suggestions, {
        keywordsProperty: "lowConfidenceKeywords",
        mapKeyword:
          lazy.SuggestionsMap.MAP_KEYWORD_PREFIXES_STARTING_AT_FIRST_WORD,
      });
      if (!this.isEnabled) {
        return;
      }

      await highMap.add(suggestions, {
        keywordsProperty: "highConfidenceKeywords",
      });
      if (!this.isEnabled) {
        return;
      }
    }

    this.#lowConfidenceSuggestionsMap = lowMap;
    this.#highConfidenceSuggestionsMap = highMap;
  }

  makeResult(queryContext, suggestion, searchString) {
    if (!this.isEnabled) {
      // The feature is disabled on the client, but Merino may still return
      // suggestions anyway, and we filter them out here.
      return null;
    }

    // If the user hasn't clicked the "Show less frequently" command, the
    // suggestion can be shown. Otherwise, the suggestion can be shown if the
    // user typed more than one word with at least `showLessFrequentlyCount`
    // characters after the first word, including spaces.
    if (this.showLessFrequentlyCount) {
      let spaceIndex = searchString.search(/\s/);
      if (
        spaceIndex < 0 ||
        searchString.length - spaceIndex < this.showLessFrequentlyCount
      ) {
        return null;
      }
    }

    if (suggestion.source == "rust") {
      suggestion.is_top_pick = suggestion.isTopPick;
      delete suggestion.isTopPick;

      // The Rust component doesn't implement these properties. For now we use
      // dummy values. See issue #5878 in application-services.
      suggestion.description = suggestion.title;
      suggestion.full_keyword = searchString;
    }

    let url = new URL(suggestion.url);
    url.searchParams.set("utm_medium", "firefox-desktop");
    url.searchParams.set("utm_source", "firefox-suggest");
    url.searchParams.set(
      "utm_campaign",
      "pocket-collections-in-the-address-bar"
    );
    url.searchParams.set("utm_content", "treatment");

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          url: url.href,
          originalUrl: suggestion.url,
          title: [suggestion.title, lazy.UrlbarUtils.HIGHLIGHT.TYPED],
          description: suggestion.is_top_pick ? suggestion.description : "",
          // Use the favicon for non-best matches so the icon exactly matches
          // the Pocket favicon in the user's history and tabs.
          icon: suggestion.is_top_pick
            ? "chrome://global/skin/icons/pocket.svg"
            : "chrome://global/skin/icons/pocket-favicon.ico",
          shouldShowUrl: true,
          bottomTextL10n: {
            id: "firefox-suggest-pocket-bottom-text",
            args: {
              keywordSubstringTyped: searchString,
              keywordSubstringNotTyped: suggestion.full_keyword.substring(
                searchString.length
              ),
            },
          },
          helpUrl: lazy.QuickSuggest.HELP_URL,
        })
      ),
      {
        isRichSuggestion: true,
        richSuggestionIconSize: suggestion.is_top_pick ? 24 : 16,
        showFeedbackMenu: true,
      }
    );
  }

  handleCommand(view, result, selType) {
    switch (selType) {
      case RESULT_MENU_COMMAND.HELP:
        // "help" is handled by UrlbarInput, no need to do anything here.
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        // PocketSuggestions adds the UTM parameters to the original URL and
        // returns it as payload.url in the result. However, as
        // UrlbarProviderQuickSuggest filters suggestions with original URL of
        // provided suggestions, need to use the original URL when adding to the
        // block list.
        lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-one",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
        lazy.UrlbarPrefs.set("suggest.pocket", false);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-all",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        view.acknowledgeFeedback(result);
        this.incrementShowLessFrequentlyCount();
        if (!this.canShowLessFrequently) {
          view.invalidateResultMenuCommands();
        }
        break;
    }
  }

  getResultCommands(result) {
    let commands = [];

    if (!result.isBestMatch && this.canShowLessFrequently) {
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

  incrementShowLessFrequentlyCount() {
    if (this.canShowLessFrequently) {
      lazy.UrlbarPrefs.set(
        "pocket.showLessFrequentlyCount",
        this.showLessFrequentlyCount + 1
      );
    }
  }

  #lowConfidenceSuggestionsMap;
  #highConfidenceSuggestionsMap;
}
