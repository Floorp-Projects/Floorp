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
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

const UTM_PARAMS = {
  utm_medium: "firefox-desktop",
  utm_source: "firefox-suggest",
};

const VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "content",
      tag: "span",
      overflowable: true,
      children: [
        {
          name: "icon",
          tag: "img",
        },
        {
          name: "header",
          tag: "span",
          children: [
            {
              name: "title",
              tag: "span",
              classList: ["urlbarView-title"],
            },
            {
              name: "separator",
              tag: "span",
              classList: ["urlbarView-title-separator"],
            },
            {
              name: "url",
              tag: "span",
              classList: ["urlbarView-url"],
            },
          ],
        },
        {
          name: "description",
          tag: "span",
        },
        {
          name: "footer",
          tag: "span",
          children: [
            {
              name: "ratingContainer",
              tag: "span",
              children: [
                {
                  classList: ["urlbarView-dynamic-addons-rating"],
                  name: "rating0",
                  tag: "span",
                },
                {
                  classList: ["urlbarView-dynamic-addons-rating"],
                  name: "rating1",
                  tag: "span",
                },
                {
                  classList: ["urlbarView-dynamic-addons-rating"],
                  name: "rating2",
                  tag: "span",
                },
                {
                  classList: ["urlbarView-dynamic-addons-rating"],
                  name: "rating3",
                  tag: "span",
                },
                {
                  classList: ["urlbarView-dynamic-addons-rating"],
                  name: "rating4",
                  tag: "span",
                },
              ],
            },
            {
              name: "reviews",
              tag: "span",
            },
          ],
        },
      ],
    },
  ],
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
  constructor() {
    super();
    lazy.UrlbarResult.addDynamicResultType("addons");
    lazy.UrlbarView.addDynamicViewTemplate("addons", VIEW_TEMPLATE);
  }

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

  enable(enabled) {
    if (enabled) {
      lazy.QuickSuggest.jsBackend.register(this);
    } else {
      lazy.QuickSuggest.jsBackend.unregister(this);
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
      rating: suggestion.rating,
      number_of_ratings: suggestion.number_of_ratings,
      guid: suggestion.guid,
      score: suggestion.score,
      is_top_pick: suggestion.is_top_pick,
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

    // If is_top_pick is not specified, handle it as top pick suggestion.
    suggestion.is_top_pick = suggestion.is_top_pick ?? true;

    const { guid, rating, number_of_ratings } =
      suggestion.source === "remote-settings"
        ? suggestion
        : suggestion.custom_details.amo;

    const addon = await lazy.AddonManager.getAddonByID(guid);
    if (addon) {
      // Addon suggested is already installed.
      return null;
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
      icon: suggestion.icon,
      url: url.href,
      originalUrl: suggestion.url,
      title: suggestion.title,
      description: suggestion.description,
      rating: Number(rating),
      reviews: Number(number_of_ratings),
      helpUrl: lazy.QuickSuggest.HELP_URL,
      shouldNavigate: true,
      dynamicType: "addons",
    };

    let result = Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.DYNAMIC,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(
          queryContext.tokens,
          payload
        )
      ),
      { showFeedbackMenu: true }
    );

    // UrlbarProviderQuickSuggest will make the result a best match only if
    // `browser.urlbar.bestMatch.enabled` is true. Addon suggestions should be
    // best matches regardless (as long as `suggestion.is_top_pick` is true), so
    // override the provider behavior by setting the related properties here.
    if (suggestion.is_top_pick) {
      result.isBestMatch = true;
      result.suggestedIndex = 1;
    }

    return result;
  }

  getViewUpdate(result) {
    const treatment = lazy.UrlbarPrefs.get("addonsUITreatment");
    const rating = result.payload.rating;

    return {
      content: {
        attributes: { treatment },
      },
      icon: {
        attributes: {
          src: result.payload.icon,
        },
      },
      url: {
        textContent: result.payload.url,
      },
      title: {
        textContent: result.payload.title,
      },
      description: {
        textContent: result.payload.description,
      },
      rating0: {
        attributes: {
          fill: this.#getRatingStar(0, rating),
        },
      },
      rating1: {
        attributes: {
          fill: this.#getRatingStar(1, rating),
        },
      },
      rating2: {
        attributes: {
          fill: this.#getRatingStar(2, rating),
        },
      },
      rating3: {
        attributes: {
          fill: this.#getRatingStar(3, rating),
        },
      },
      rating4: {
        attributes: {
          fill: this.#getRatingStar(4, rating),
        },
      },
      reviews: {
        l10n:
          treatment === "b"
            ? { id: "firefox-suggest-addons-recommended" }
            : {
                id: "firefox-suggest-addons-reviews",
                args: {
                  quantity: result.payload.reviews,
                },
              },
      },
    };
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

  #getRatingStar(nth, rating) {
    // 0    <= x <  0.25 = empty
    // 0.25 <= x <  0.75 = half
    // 0.75 <= x <= 1    = full
    // ... et cetera, until x <= 5.
    const distanceToFull = rating - nth;
    if (distanceToFull < 0.25) {
      return "empty";
    }
    if (distanceToFull < 0.75) {
      return "half";
    }
    return "full";
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
