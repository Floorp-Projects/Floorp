/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  QuickSuggestRemoteSettings:
    "resource:///modules/urlbar/private/QuickSuggestRemoteSettings.sys.mjs",
  SuggestionsMap:
    "resource:///modules/urlbar/private/QuickSuggestRemoteSettings.sys.mjs",
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
    this.#suggestionsMap = new lazy.SuggestionsMap();
  }

  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("quickSuggestRemoteSettingsEnabled") &&
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

  get showLessFrequentlyCount() {
    let count = lazy.UrlbarPrefs.get("pocket.showLessFrequentlyCount") || 0;
    return Math.max(count, 0);
  }

  get canShowLessFrequently() {
    // TODO (bug 1837097): To be implemented once the "Show less frequently"
    // logic is decided.
    return false;
  }

  enable(enabled) {
    if (enabled) {
      lazy.QuickSuggestRemoteSettings.register(this);
    } else {
      lazy.QuickSuggestRemoteSettings.unregister(this);
    }
  }

  async queryRemoteSettings(searchString) {
    let suggestions = this.#suggestionsMap.get(searchString);
    if (!suggestions) {
      return [];
    }

    return suggestions.map(suggestion => ({
      url: suggestion.url,
      title: suggestion.title,
      score: suggestion.score,
      is_top_pick: suggestion.is_top_pick,
    }));
  }

  async onRemoteSettingsSync(rs) {
    let records = await rs.get({ filters: { type: "pocket-suggestions" } });
    if (rs != lazy.QuickSuggestRemoteSettings.rs) {
      return;
    }

    let suggestionsMap = new lazy.SuggestionsMap();

    this.logger.debug(`Got ${records.length} records`);
    for (let record of records) {
      let { buffer } = await rs.attachments.download(record);
      if (rs != lazy.QuickSuggestRemoteSettings.rs) {
        return;
      }

      let suggestions = JSON.parse(new TextDecoder("utf-8").decode(buffer));
      this.logger.debug(`Adding ${suggestions.length} suggestions`);
      await suggestionsMap.add(suggestions);
      if (rs != lazy.QuickSuggestRemoteSettings.rs) {
        return;
      }
    }

    this.#suggestionsMap = suggestionsMap;
  }

  makeResult(queryContext, suggestion, searchString) {
    // If `is_top_pick` is not specified, handle it as top pick suggestion.
    suggestion.is_top_pick = suggestion.is_top_pick ?? true;

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          url: suggestion.url,
          title: [suggestion.title, lazy.UrlbarUtils.HIGHLIGHT.TYPED],
          icon: "chrome://global/skin/icons/pocket.svg",
          helpUrl: lazy.QuickSuggest.HELP_URL,
        })
      ),
      { showFeedbackMenu: true }
    );
  }

  handleCommand(queryContext, result, selType) {
    switch (selType) {
      case RESULT_MENU_COMMAND.HELP:
        // "help" is handled by UrlbarInput, no need to do anything here.
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        lazy.QuickSuggest.blockedSuggestions.add(result.payload.url);
        queryContext.view.acknowledgeDismissal(result);
        break;
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        queryContext.view.acknowledgeFeedback(result);
        this.#incrementShowLessFrequentlyCount();
        break;
    }
  }

  getResultCommands(result) {
    let commands = [];

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

  #incrementShowLessFrequentlyCount() {
    if (this.canShowLessFrequently) {
      lazy.UrlbarPrefs.set(
        "pocket.showLessFrequentlyCount",
        this.showLessFrequentlyCount + 1
      );
    }
  }

  #suggestionsMap;
}
