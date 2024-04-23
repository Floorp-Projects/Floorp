/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A provider that matches the urlbar input to built in actions.
 */
export class ActionsProvider {
  /**
   * Unique name for the provider.
   *
   * @abstract
   */
  get name() {
    return "ActionsProviderBase";
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
   * @param {UrlbarQueryContext} _queryContext The query context object.
   * @returns {boolean} Whether this provider should be invoked for the search.
   * @abstract
   */
  isActive(_queryContext) {
    throw new Error("Not implemented.");
  }

  /**
   * Query for actions based on the current users input.
   *
   * @param {UrlbarQueryContext} _queryContext The query context object.
   * @param {UrlbarController} _controller The urlbar controller.
   * @returns {ActionsResult}
   * @abstract
   */
  async queryAction(_queryContext, _controller) {
    throw new Error("Not implemented.");
  }

  /**
   * Pick an action.
   *
   * @param {UrlbarQueryContext} _queryContext The query context object.
   * @param {UrlbarController} _controller The urlbar controller.
   * @param {DOMElement} _element The element that was selected.
   * @abstract
   */
  pickAction(_queryContext, _controller, _element) {
    throw new Error("Not implemented.");
  }
}

/**
 * Class used to create an Actions Result.
 */
export class ActionsResult {
  providerName;

  #key;
  #l10nId;
  #l10nArgs;
  #icon;
  #dataset;

  /**
   * @param {object} options
   *    An option object.
   * @param { string } options.key
   *    A string key used to distinguish between different actions.
   * @param { string } options.l10nId
   *    The id of the l10n string displayed in the action button.
   * @param { string } options.l10nArgs
   *    Arguments passed to construct the above string
   * @param { string } options.icon
   *    The icon displayed in the button.
   * @param {object} options.dataset
   *    An object of properties we set on the action button that
   *    can be used to pass data when it is selected.
   */
  constructor({ key, l10nId, l10nArgs, icon, dataset }) {
    this.#key = key;
    this.#l10nId = l10nId;
    this.#l10nArgs = l10nArgs;
    this.#icon = icon;
    this.#dataset = dataset;
  }

  get key() {
    return this.#key;
  }

  get l10nId() {
    return this.#l10nId;
  }

  get l10nArgs() {
    return this.#l10nArgs;
  }

  get icon() {
    return this.#icon;
  }

  get dataset() {
    return this.#dataset;
  }
}
