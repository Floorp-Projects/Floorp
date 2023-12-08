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
};

/**
 * A feature that supports MDN suggestions.
 */
export class MDNSuggestions extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("mdn.featureGate") &&
      lazy.UrlbarPrefs.get("suggest.mdn") &&
      lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored")
    );
  }

  get enablingPreferences() {
    return [
      "mdn.featureGate",
      "suggest.mdn",
      "suggest.quicksuggest.nonsponsored",
    ];
  }

  get merinoProvider() {
    return "mdn";
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
    return suggestions
      ? suggestions.map(suggestion => ({ ...suggestion }))
      : [];
  }

  async onRemoteSettingsSync(rs) {
    const records = await rs.get({ filters: { type: "mdn-suggestions" } });
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
      // mdn suggestions anyway, and we filter them out here.
      return null;
    }

    // Set `is_top_pick` on the suggestion to tell the provider to set
    // best-match related properties on the result.
    suggestion.is_top_pick = true;

    const url = new URL(suggestion.url);
    url.searchParams.set("utm_medium", "firefox-desktop");
    url.searchParams.set("utm_source", "firefox-suggest");
    url.searchParams.set(
      "utm_campaign",
      "firefox-mdn-web-docs-suggestion-experiment"
    );
    url.searchParams.set("utm_content", "treatment");

    const payload = {
      icon: "chrome://global/skin/icons/mdn.svg",
      url: url.href,
      originalUrl: suggestion.url,
      title: [suggestion.title, lazy.UrlbarUtils.HIGHLIGHT.TYPED],
      description: suggestion.description,
      shouldShowUrl: true,
      bottomTextL10n: { id: "firefox-suggest-mdn-bottom-text" },
    };

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(
          queryContext.tokens,
          payload
        )
      ),
      { showFeedbackMenu: true }
    );
  }

  getResultCommands(result) {
    return [
      {
        l10n: {
          id: "firefox-suggest-command-dont-show-mdn",
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
      },
    ];
  }

  handleCommand(view, result, selType) {
    switch (selType) {
      case RESULT_MENU_COMMAND.HELP:
        // "help" is handled by UrlbarInput, no need to do anything here.
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        // MDNSuggestions adds the UTM parameters to the original URL and
        // returns it as payload.url in the result. However, as
        // UrlbarProviderQuickSuggest filters suggestions with original URL of
        // provided suggestions, need to use the original URL when adding to the
        // block list.
        lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-one-mdn",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
        lazy.UrlbarPrefs.set("suggest.mdn", false);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-all-mdn",
        };
        view.controller.removeResult(result);
        break;
    }
  }

  #suggestionsMap = null;
}
