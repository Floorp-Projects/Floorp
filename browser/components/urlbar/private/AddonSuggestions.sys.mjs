/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  SuggestionsMap: "resource:///modules/urlbar/private/SuggestBackendJs.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const UTM_PARAMS = {
  utm_medium: "firefox-desktop",
  utm_source: "firefox-suggest",
};

const RESULT_MENU_COMMAND = {
  HELP: "help",
  NOT_INTERESTED: "not_interested",
  NOT_RELEVANT: "not_relevant",
  SHOW_LESS_FREQUENTLY: "show_less_frequently",
};

/**
 * A feature that supports Addon suggestions.
 */
export class AddonSuggestions extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("addonsFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.addons") &&
      lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored")
    );
  }

  get enablingPreferences() {
    return ["suggest.addons", "suggest.quicksuggest.nonsponsored"];
  }

  get merinoProvider() {
    return "amo";
  }

  get rustSuggestionTypes() {
    return ["Amo"];
  }

  enable(enabled) {
    if (enabled) {
      lazy.QuickSuggest.jsBackend.register(this);
    } else {
      lazy.QuickSuggest.jsBackend.unregister(this);
      this.#suggestionsMap?.clear();
    }
  }

  queryRemoteSettings(searchString) {
    const suggestions = this.#suggestionsMap?.get(searchString);
    if (!suggestions) {
      return [];
    }

    return suggestions.map(suggestion => ({
      icon: suggestion.icon,
      url: suggestion.url,
      title: suggestion.title,
      description: suggestion.description,
      guid: suggestion.guid,
      score: suggestion.score,
    }));
  }

  async onRemoteSettingsSync(rs) {
    const records = await rs.get({ filters: { type: "amo-suggestions" } });
    if (!this.isEnabled) {
      return;
    }

    const suggestionsMap = new lazy.SuggestionsMap();

    for (const record of records) {
      const { buffer } = await rs.attachments.download(record);
      if (!this.isEnabled) {
        return;
      }

      const results = JSON.parse(new TextDecoder("utf-8").decode(buffer));
      await suggestionsMap.add(results, {
        mapKeyword:
          lazy.SuggestionsMap.MAP_KEYWORD_PREFIXES_STARTING_AT_FIRST_WORD,
      });
      if (!this.isEnabled) {
        return;
      }
    }

    this.#suggestionsMap = suggestionsMap;
  }

  async makeResult(queryContext, suggestion, searchString) {
    if (!this.isEnabled) {
      // The feature is disabled on the client, but Merino may still return
      // addon suggestions anyway, and we filter them out here.
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

    const { guid } =
      suggestion.source === "merino"
        ? suggestion.custom_details.amo
        : suggestion;

    const addon = await lazy.AddonManager.getAddonByID(guid);
    if (addon) {
      // Addon suggested is already installed.
      return null;
    }

    if (suggestion.source == "rust") {
      suggestion.icon = suggestion.iconUrl;
      delete suggestion.iconUrl;
    }

    // Set UTM params unless they're already defined. This allows remote
    // settings or Merino to override them if need be.
    let url = new URL(suggestion.url);
    for (let [key, value] of Object.entries(UTM_PARAMS)) {
      if (!url.searchParams.has(key)) {
        url.searchParams.set(key, value);
      }
    }

    const payload = {
      url: url.href,
      originalUrl: suggestion.url,
      shouldShowUrl: true,
      title: suggestion.title,
      description: suggestion.description,
      bottomTextL10n: { id: "firefox-suggest-addons-recommended" },
      helpUrl: lazy.QuickSuggest.HELP_URL,
    };

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(
          queryContext.tokens,
          payload
        )
      ),
      {
        isBestMatch: true,
        suggestedIndex: 1,
        isRichSuggestion: true,
        richSuggestionIconSize: 24,
        showFeedbackMenu: true,
      }
    );
  }

  getResultCommands(result) {
    const commands = [];

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

  handleCommand(view, result, selType) {
    switch (selType) {
      case RESULT_MENU_COMMAND.HELP:
        // "help" is handled by UrlbarInput, no need to do anything here.
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-one",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
        lazy.UrlbarPrefs.set("suggest.addons", false);
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

  incrementShowLessFrequentlyCount() {
    if (this.canShowLessFrequently) {
      lazy.UrlbarPrefs.set(
        "addons.showLessFrequentlyCount",
        this.showLessFrequentlyCount + 1
      );
    }
  }

  get showLessFrequentlyCount() {
    const count = lazy.UrlbarPrefs.get("addons.showLessFrequentlyCount") || 0;
    return Math.max(count, 0);
  }

  get canShowLessFrequently() {
    const cap =
      lazy.UrlbarPrefs.get("addonsShowLessFrequentlyCap") ||
      lazy.QuickSuggest.jsBackend.config.show_less_frequently_cap ||
      0;
    return !cap || this.showLessFrequentlyCount < cap;
  }

  #suggestionsMap = null;
}
