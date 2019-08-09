/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UrlbarContextualTip"];

/**
 * Consumers of this class can create, set, remove, and hide a contextual tip.
 */
class UrlbarContextualTip {
  /**
   * Creates the contextual tip and sets it in the urlbar's view.
   * @param {object} view The urlbar's view
   */
  constructor(view) {
    this.document = view.document;
    this.view = view;
    this._create();
  }

  /**
   * Creates an empty contextual tip in the DOM.
   */
  _create() {
    this._elements = {};

    const fragment = this.document.createDocumentFragment();

    this._elements.container = this.document.createElement("div");
    this._elements.container.id = "urlbar-contextual-tip";

    this._elements.icon = this.document.createElement("div");
    this._elements.icon.id = "urlbar-contextual-tip-icon";

    this._elements.title = this.document.createElement("div");
    this._elements.title.id = "urlbar-contextual-tip-title";

    this._elements.button = this.document.createElement("button");
    this._elements.button.id = "urlbar-contextual-tip-button";

    this._elements.link = this.document.createElement("a");
    this._elements.link.id = "urlbar-contextual-tip-link";

    this._elements.container.appendChild(this._elements.icon);
    this._elements.container.appendChild(this._elements.title);
    this._elements.container.appendChild(this._elements.button);
    this._elements.container.appendChild(this._elements.link);

    fragment.appendChild(this._elements.container);
    this.view.panel.prepend(fragment);
  }

  /**
   * Removes the contextual tip from the DOM.
   */
  remove() {
    this._elements.container.remove();
    this._elements = null;
  }

  /**
   * Sets the title, button's title, and link's title
   * for the contextual tip.
   *
   * @param {object} details
   * @param {string} details.title
   *   Main title displayed by the contextual tip.
   * @param {string} [details.buttonTitle]
   *   Title of the button on the contextual tip.
   *   If omitted then the button will be hidden.
   * @param {string} [details.linkTitle]
   *   Title of the link on the contextual tip.
   *   If omitted then the link will be hidden.
   */
  set(details) {
    let { title, buttonTitle, linkTitle } = details;

    this._elements.title.textContent = title;

    this._elements.button.textContent = buttonTitle;
    this._elements.button.classList.toggle("hidden", !buttonTitle);

    this._elements.link.textContent = linkTitle;
    this._elements.link.classList.toggle("hidden", !linkTitle);

    // If the contextual tip is not visible, then we'll make it visible
    this._elements.container.classList.remove("hidden");
  }

  /**
   * Hides the contextual tip.
   */
  hide() {
    if (!this._elements.container.classList.contains("hidden")) {
      this._elements.container.classList.add("hidden");
    }
  }
}
