/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarProviderQuickActions"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { UrlbarProvider, UrlbarUtils } = ChromeUtils.import(
  "resource:///modules/UrlbarUtils.jsm"
);

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  QuickActionsLoaderDefault:
    "resource:///modules/QuickActionsLoaderDefault.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
});

// These prefs are relative to the `browser.urlbar` branch.
const ENABLED_PREF = "quickactions.enabled";
const DYNAMIC_TYPE_NAME = "quickactions";

// When the urlbar is first focused and no search term has been
// entered we show a limited number of results.
const ACTIONS_SHOWN_FOCUS = 5;

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
   * @returns {string} the name of this provider.
   */
  get name() {
    return DYNAMIC_TYPE_NAME;
  }

  /**
   * The type of the provider.
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
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    return lazy.UrlbarPrefs.get(ENABLED_PREF);
  }

  /**
   * Starts querying.
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @note Extended classes should return a Promise resolved when the provider
   *       is done searching AND returning results.
   */
  async startQuery(queryContext, addCallback) {
    let results = this.#keywords.get(
      queryContext.trimmedSearchString.toLowerCase()
    );
    results = results?.filter(key => {
      let action = this.#actions.get(key);
      return !("isActive" in action) || !action.isActive();
    });
    if (!results?.length) {
      return;
    }

    // If we are in the Actions searchMode then we want to show all the actions
    // but not when we are in the normal url mode on first focus.
    if (
      results.length > ACTIONS_SHOWN_FOCUS &&
      !queryContext.searchString &&
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
      }
    );
    result.suggestedIndex = SUGGESTED_INDEX;
    addCallback(this, result);
  }

  getViewTemplate(result) {
    return {
      children: result.payload.results.map((el, i) => ({
        name: `button-${i}`,
        tag: "span",
        attributes: {
          class: "urlbarView-quickaction-row",
          role: "button",
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
                attributes: { class: "urlbarView-favicon-img" },
              },
            ],
          },
          {
            name: `div-${i}`,
            tag: "div",
            children: [
              {
                name: `label-${i}`,
                tag: "span",
                attributes: { class: "urlbarView-label" },
              },
            ],
          },
        ],
      })),
    };
  }

  getViewUpdate(result) {
    let viewUpdate = {};
    result.payload.results.forEach(({ key }, i) => {
      let data = this.#actions.get(key) || { icon: "", label: " " };
      let buttonAttributes = { "data-key": key };
      viewUpdate[`button-${i}`] = { attributes: buttonAttributes };
      viewUpdate[`image-${i}`] = {
        attributes: { src: data.icon || DEFAULT_ICON },
      };
      viewUpdate[`label-${i}`] = { attributes: { "data-l10n-id": data.label } };
    });
    return viewUpdate;
  }

  pickResult(result, itemPicked) {
    this.#actions.get(itemPicked.dataset.key).onPick();
  }

  /**
   * Adds a new QuickAction.
   * @param {string} key A key to identify this action.
   * @param {string} definition An object that describes the action.
   */
  addAction(key, definition) {
    this.#actions.set(key, definition);
    this.#loopOverPrefixes(definition.commands, prefix => {
      let result = this.#keywords.get(prefix);
      if (result) {
        if (!result.includes(key)) {
          result.push(key);
        }
      } else {
        result = [key];
      }
      this.#keywords.set(prefix, result);
    });
  }

  /**
   * Removes an action.
   * @param {string} key A key to identify this action.
   */
  removeAction(key) {
    let definition = this.#actions.get(key);
    this.#actions.delete(key);
    this.#loopOverPrefixes(definition.commands, prefix => {
      let result = this.#keywords.get(prefix);
      if (result) {
        result = result.filter(val => val == key);
      }
      this.#keywords.set(prefix, result);
    });
  }

  // A map from keywords to an action.
  #keywords = new Map();

  // The actions that have been added.
  #actions = new Map();

  #loopOverPrefixes(commands, fun) {
    for (const command of commands) {
      for (let i = 0; i <= command.length; i++) {
        let prefix = command.substring(0, command.length - i);
        fun(prefix);
      }
    }
  }
}

var UrlbarProviderQuickActions = new ProviderQuickActions();
