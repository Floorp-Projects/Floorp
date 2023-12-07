/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module exports a provider that offers a search engine when the user is
 * typing a search engine domain.
 */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

const DYNAMIC_RESULT_TYPE = "quickSuggestContextualOptIn";
const VIEW_TEMPLATE = {
  children: [
    {
      name: "no-wrap",
      tag: "span",
      classList: ["urlbarView-no-wrap"],
      children: [
        {
          name: "icon",
          tag: "img",
          classList: ["urlbarView-favicon"],
        },
        {
          name: "text-container",
          tag: "span",
          children: [
            {
              name: "title",
              tag: "strong",
            },
            {
              name: "description",
              tag: "span",
              children: [
                {
                  name: "learn_more",
                  tag: "a",
                  attributes: {
                    "data-l10n-name": "learn-more-link",
                    selectable: true,
                  },
                },
              ],
            },
          ],
        },
      ],
    },
  ],
};

/**
 * Initializes this provider's dynamic result. To be called after the creation
 * of the provider singleton.
 */
function initializeDynamicResult() {
  lazy.UrlbarResult.addDynamicResultType(DYNAMIC_RESULT_TYPE);
  lazy.UrlbarView.addDynamicViewTemplate(DYNAMIC_RESULT_TYPE, VIEW_TEMPLATE);
}

/**
 * Class used to create the provider.
 */
class ProviderQuickSuggestContextualOptIn extends UrlbarProvider {
  constructor() {
    super();
  }

  get name() {
    return "UrlbarProviderQuickSuggestContextualOptIn";
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.HEURISTIC;
  }

  _shouldDisplayContextualOptIn(queryContext = null) {
    if (
      queryContext &&
      (queryContext.isPrivate ||
        queryContext.restrictSource ||
        queryContext.searchString ||
        queryContext.searchMode)
    ) {
      return false;
    }

    // If the feature is disabled, or the user has already opted in, don't show
    // the onboarding.
    if (
      !lazy.UrlbarPrefs.get("quickSuggestEnabled") ||
      !lazy.UrlbarPrefs.get("quicksuggest.contextualOptIn") ||
      lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled")
    ) {
      return false;
    }

    let lastDismissed = lazy.UrlbarPrefs.get(
      "quicksuggest.contextualOptIn.lastDismissed"
    );
    if (lastDismissed) {
      let fourteenDays = 14 * 24 * 60 * 60 * 1000;
      if (new Date() - new Date(lastDismissed) < fourteenDays) {
        return false;
      }
    }

    return true;
  }

  isActive(queryContext) {
    return (
      this._shouldDisplayContextualOptIn(queryContext) &&
      lazy.UrlbarPrefs.get("quicksuggest.contextualOptIn.topPosition")
    );
  }

  getPriority(queryContext) {
    return lazy.UrlbarProviderTopSites.PRIORITY;
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
   *
   * @param {UrlbarResult} result The result whose view will be updated.
   * @param {Map} idsByName
   *   A Map from an element's name, as defined by the provider; to its ID in
   *   the DOM, as defined by the browser.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result, idsByName) {
    let alternativeCopy = lazy.UrlbarPrefs.get(
      "quicksuggest.contextualOptIn.sayHello"
    );
    return {
      icon: {
        attributes: {
          src: "chrome://branding/content/icon32.png",
        },
      },
      title: {
        l10n: {
          id: alternativeCopy
            ? "urlbar-firefox-suggest-contextual-opt-in-title-2"
            : "urlbar-firefox-suggest-contextual-opt-in-title-1",
        },
      },
      description: {
        l10n: {
          id: alternativeCopy
            ? "urlbar-firefox-suggest-contextual-opt-in-description-2"
            : "urlbar-firefox-suggest-contextual-opt-in-description-1",
        },
      },
    };
  }

  onBeforeSelection(result, element) {
    if (element.getAttribute("name") == "learn_more") {
      this.#a11yAlertRow(element.closest(".urlbarView-row"));
    }
  }

  #a11yAlertRow(row) {
    let alertText = row.querySelector(
      ".urlbarView-dynamic-quickSuggestContextualOptIn-title"
    ).textContent;
    let decription = row
      .querySelector(
        ".urlbarView-dynamic-quickSuggestContextualOptIn-description"
      )
      .cloneNode(true);
    // Remove the "Learn More" link.
    decription.firstElementChild?.remove();
    alertText += ". " + decription.textContent;
    row.ownerGlobal.A11yUtils.announce({ raw: alertText });
  }

  onEngagement(state, queryContext, details, controller) {
    let { result } = details;
    if (result?.providerName != this.name) {
      return;
    }
    this._handleCommand(details.element, controller, result);
  }

  _handleCommand(element, controller, result, container) {
    let commandName = element?.getAttribute("name");
    switch (commandName) {
      case "learn_more":
        controller.browserWindow.openHelpLink("firefox-suggest");
        break;
      case "allow":
        lazy.UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
        break;
      case "dismiss":
        lazy.UrlbarPrefs.set(
          "quicksuggest.contextualOptIn.lastDismissed",
          new Date().toISOString()
        );
        break;
      default:
        return;
    }

    this._recordGlean(commandName);

    // Remove the result if it shouldn't be active anymore due to above
    // actions.
    if (!this._shouldDisplayContextualOptIn()) {
      if (result) {
        controller.removeResult(result);
      } else {
        // This is for when the UI is outside of standard results, after
        // one-off search buttons.
        container.hidden = true;
      }
    }
  }

  /**
   * Starts querying.
   *
   * @param {object} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        result.
   * @returns {Promise} resolved when the query stops.
   */
  async startQuery(queryContext, addCallback) {
    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        buttons: [
          {
            l10n: {
              id: "urlbar-firefox-suggest-contextual-opt-in-allow",
            },
            attributes: { primary: true, name: "allow" },
          },
          {
            l10n: {
              id: "urlbar-firefox-suggest-contextual-opt-in-dismiss",
            },
            attributes: { name: "dismiss" },
          },
        ],
        dynamicType: DYNAMIC_RESULT_TYPE,
      }
    );
    result.suggestedIndex = 0;
    addCallback(this, result);

    this._recordGlean("impression");
  }

  _recordGlean(interaction) {
    Glean.urlbar.quickSuggestContextualOptIn.record({
      interaction,
      top_position: lazy.UrlbarPrefs.get(
        "quicksuggest.contextualOptIn.topPosition"
      ),
      say_hello: lazy.UrlbarPrefs.get("quicksuggest.contextualOptIn.sayHello"),
    });
  }
}

export var UrlbarProviderQuickSuggestContextualOptIn =
  new ProviderQuickSuggestContextualOptIn();
initializeDynamicResult();
