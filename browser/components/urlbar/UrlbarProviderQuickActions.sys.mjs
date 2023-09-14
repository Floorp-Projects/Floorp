/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  QuickActionsLoaderDefault:
    "resource:///modules/QuickActionsLoaderDefault.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

// These prefs are relative to the `browser.urlbar` branch.
const ENABLED_PREF = "quickactions.enabled";
const SUGGEST_PREF = "suggest.quickactions";
const MATCH_IN_PHRASE_PREF = "quickactions.matchInPhrase";
const MIN_SEARCH_PREF = "quickactions.minimumSearchString";
const DYNAMIC_TYPE_NAME = "quickactions";

// When the urlbar is first focused and no search term has been
// entered we show a limited number of results.
const ACTIONS_SHOWN_FOCUS = 4;

// Default icon shown for actions if no custom one is provided.
const DEFAULT_ICON = "chrome://global/skin/icons/settings.svg";

// The suggestion index of the actions row within the urlbar results.
const SUGGESTED_INDEX = 1;

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderQuickActions extends UrlbarProvider {
  constructor() {
    super();
    lazy.UrlbarResult.addDynamicResultType(DYNAMIC_TYPE_NAME);
    Services.tm.idleDispatchToMainThread(() =>
      lazy.QuickActionsLoaderDefault.load()
    );
  }

  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return DYNAMIC_TYPE_NAME;
  }

  /**
   * The type of the provider.
   *
   * @returns {UrlbarUtils.PROVIDER_TYPE}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  getPriority(context) {
    if (!context.searchString) {
      return 1;
    }
    return 0;
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
    return (
      queryContext.trimmedSearchString.length < 50 &&
      lazy.UrlbarPrefs.get(ENABLED_PREF) &&
      ((lazy.UrlbarPrefs.get(SUGGEST_PREF) && !queryContext.searchMode) ||
        queryContext.searchMode?.source == UrlbarUtils.RESULT_SOURCE.ACTIONS)
    );
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
    let input = queryContext.trimmedSearchString.toLowerCase();

    if (
      !queryContext.searchMode &&
      input.length < lazy.UrlbarPrefs.get(MIN_SEARCH_PREF)
    ) {
      return;
    }

    let results = [...(this.#prefixes.get(input) ?? [])];

    if (lazy.UrlbarPrefs.get(MATCH_IN_PHRASE_PREF)) {
      for (let [keyword, key] of this.#keywords) {
        if (input.includes(keyword)) {
          results.push(key);
        }
      }
    }
    // Ensure results are unique.
    results = [...new Set(results)];

    // Remove invisible actions.
    results = results.filter(key => {
      const action = this.#actions.get(key);
      return !action.isVisible || action.isVisible();
    });

    if (!results?.length) {
      return;
    }

    // If all actions are inactive, don't show anything.
    if (
      results.every(key => {
        const action = this.#actions.get(key);
        return action.isActive && !action.isActive();
      })
    ) {
      return;
    }

    // If we are in the Actions searchMode then we want to show all the actions
    // but not when we are in the normal url mode on first focus.
    if (
      results.length > ACTIONS_SHOWN_FOCUS &&
      !input &&
      !queryContext.searchMode
    ) {
      results.length = ACTIONS_SHOWN_FOCUS;
    }

    const result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.ACTIONS,
      {
        results: results.map(key => ({ key })),
        dynamicType: DYNAMIC_TYPE_NAME,
        inputLength: input.length,
        inQuickActionsSearchMode:
          queryContext.searchMode?.source == UrlbarUtils.RESULT_SOURCE.ACTIONS,
      }
    );
    result.suggestedIndex = SUGGESTED_INDEX;
    addCallback(this, result);
    this.#resultFromLastQuery = result;
  }

  getViewTemplate(result) {
    return {
      children: [
        {
          name: "buttons",
          tag: "div",
          attributes: {
            "data-is-quickactions-searchmode":
              result.payload.inQuickActionsSearchMode,
          },
          children: result.payload.results.map(({ key }, i) => {
            let action = this.#actions.get(key);
            let inActive = "isActive" in action && !action.isActive();
            return {
              name: `button-${i}`,
              tag: "span",
              attributes: {
                "data-key": key,
                "data-input-length": result.payload.inputLength,
                class: "urlbarView-quickaction-button",
                role: inActive ? "" : "button",
                disabled: inActive,
              },
              children: [
                {
                  name: `icon-${i}`,
                  tag: "div",
                  attributes: { class: "urlbarView-favicon" },
                  children: [
                    {
                      name: `image-${i}`,
                      tag: "img",
                      attributes: {
                        class: "urlbarView-favicon-img",
                        src: action.icon || DEFAULT_ICON,
                      },
                    },
                  ],
                },
                {
                  name: `label-${i}`,
                  tag: "span",
                  attributes: { class: "urlbarView-label" },
                },
              ],
            };
          }),
        },
      ],
    };
  }

  getViewUpdate(result) {
    let viewUpdate = {};
    result.payload.results.forEach(({ key }, i) => {
      let action = this.#actions.get(key);
      viewUpdate[`label-${i}`] = {
        l10n: { id: action.label, cacheable: true },
      };
    });
    return viewUpdate;
  }

  #pickResult(result, itemPicked) {
    let { key, inputLength } = itemPicked.dataset;
    // We clamp the input length to limit the number of keys to
    // the number of actions * 10.
    inputLength = Math.min(inputLength, 10);
    Services.telemetry.keyedScalarAdd(
      `quickaction.picked`,
      `${key}-${inputLength}`,
      1
    );
    let options = this.#actions.get(itemPicked.dataset.key).onPick() ?? {};
    if (options.focusContent) {
      itemPicked.ownerGlobal.gBrowser.selectedBrowser.focus();
    }
  }

  onEngagement(state, queryContext, details, controller) {
    // Ignore engagements on other results that didn't end the session.
    if (details.result?.providerName != this.name && details.isSessionOngoing) {
      return;
    }

    if (state == "engagement" && queryContext) {
      // Get the result that's visible in the view. `details.result` is the
      // engaged result, if any; if it's from this provider, then that's the
      // visible result. Otherwise fall back to #getVisibleResultFromLastQuery.
      let { result } = details;
      if (result?.providerName != this.name) {
        result = this.#getVisibleResultFromLastQuery(controller.view);
      }

      result?.payload.results.forEach(({ key }) => {
        Services.telemetry.keyedScalarAdd(
          `quickaction.impression`,
          `${key}-${queryContext.trimmedSearchString.length}`,
          1
        );
      });
    }

    // Handle picks.
    if (details.result?.providerName == this.name) {
      this.#pickResult(details.result, details.element);
    }

    this.#resultFromLastQuery = null;
  }

  /**
   * Adds a new QuickAction.
   *
   * @param {string} key A key to identify this action.
   * @param {string} definition An object that describes the action.
   */
  addAction(key, definition) {
    this.#actions.set(key, definition);
    definition.commands.forEach(cmd => this.#keywords.set(cmd, key));
    this.#loopOverPrefixes(definition.commands, prefix => {
      let result = this.#prefixes.get(prefix);
      if (result) {
        if (!result.includes(key)) {
          result.push(key);
        }
      } else {
        result = [key];
      }
      this.#prefixes.set(prefix, result);
    });
  }

  /**
   * Removes an action.
   *
   * @param {string} key A key to identify this action.
   */
  removeAction(key) {
    let definition = this.#actions.get(key);
    this.#actions.delete(key);
    definition.commands.forEach(cmd => this.#keywords.delete(cmd));
    this.#loopOverPrefixes(definition.commands, prefix => {
      let result = this.#prefixes.get(prefix);
      if (result) {
        result = result.filter(val => val != key);
      }
      this.#prefixes.set(prefix, result);
    });
  }

  // A map from keywords to an action.
  #keywords = new Map();

  // A map of all prefixes to an array of actions.
  #prefixes = new Map();

  // The actions that have been added.
  #actions = new Map();

  // The result we added during the most recent query.
  #resultFromLastQuery = null;

  #loopOverPrefixes(commands, fun) {
    for (const command of commands) {
      // Loop over all the prefixes of the word, ie
      // "", "w", "wo", "wor", stopping just before the full
      // word itself which will be matched by the whole
      // phrase matching.
      for (let i = 1; i <= command.length; i++) {
        let prefix = command.substring(0, command.length - i);
        fun(prefix);
      }
    }
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
}

export var UrlbarProviderQuickActions = new ProviderQuickActions();
