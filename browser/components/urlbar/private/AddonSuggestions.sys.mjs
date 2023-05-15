/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

const VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "content",
      tag: "span",
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
    return lazy.UrlbarPrefs.get("addons.featureGate");
  }

  get enablingPreferences() {
    return ["addons.featureGate"];
  }

  makeResult(queryContext, suggestion, searchString) {
    if (!this.isEnabled || searchString.length < this.#minKeywordLength) {
      return null;
    }

    const payload = {
      source: suggestion.source,
      icon: suggestion.icon,
      url: suggestion.url,
      title: suggestion.title,
      description: suggestion.description,
      rating: Number(suggestion.custom_details.amo.rating),
      reviews: Number(suggestion.custom_details.amo.number_of_ratings),
      helpUrl: lazy.QuickSuggest.HELP_URL,
      shouldNavigate: true,
      dynamicType: "addons",
      telemetryType: "amo",
    };

    return new lazy.UrlbarResult(
      lazy.UrlbarUtils.RESULT_TYPE.DYNAMIC,
      lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );
  }

  getViewUpdate(result) {
    const rating = result.payload.rating;
    return {
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
        l10n: {
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

    if (this.#canIncrementMinKeywordLength) {
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

  handlePossibleCommand(queryContext, result, selType) {
    switch (selType) {
      case RESULT_MENU_COMMAND.HELP:
        // "help" is handled by UrlbarInput, no need to do anything here.
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        lazy.UrlbarPrefs.set("addons.featureGate", false);
        queryContext.view.acknowledgeDismissal(result);
        break;
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        queryContext.view.acknowledgeFeedback(result);
        this.#incrementMinKeywordLength();
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

  #incrementMinKeywordLength() {
    if (this.#canIncrementMinKeywordLength) {
      lazy.UrlbarPrefs.set(
        "addons.minKeywordLength",
        this.#minKeywordLength + 1
      );
    }
  }

  get #minKeywordLength() {
    const minLength =
      lazy.UrlbarPrefs.get("addons.minKeywordLength") ||
      lazy.UrlbarPrefs.get("addonsKeywordsMinimumLength") ||
      0;
    return Math.max(minLength, 0);
  }

  get #canIncrementMinKeywordLength() {
    const cap = lazy.UrlbarPrefs.get("addonsKeywordsMinimumLengthCap") || 0;
    return !cap || this.#minKeywordLength < cap;
  }
}
