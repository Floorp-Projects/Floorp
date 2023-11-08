/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * A search box that displays a search icon and is clearable. Updates to the
 * search query trigger a `fxview-search-textbox-query` event with the current
 * query value.
 *
 * @property {string} placeholder
 *   The placeholder text for the search box.
 * @property {string} query
 *   The query that is currently in the search box.
 */
export default class FxviewSearchTextbox extends MozLitElement {
  static properties = {
    placeholder: { type: String },
    query: { type: String },
  };

  static queries = {
    clearButton: ".clear-icon",
  };

  constructor() {
    super();
    this.query = "";
  }

  onInput(event) {
    this.query = event.target.value.trim();
    event.preventDefault();
    this.#dispatchQueryEvent();
  }

  clear(event) {
    this.query = "";
    event.preventDefault();
    this.#dispatchQueryEvent();
  }

  #dispatchQueryEvent() {
    this.dispatchEvent(
      new CustomEvent("fxview-search-textbox-query", {
        bubbles: true,
        composed: true,
        detail: { query: this.query },
      })
    );
  }

  render() {
    return html`
    <link rel="stylesheet" href="chrome://browser/content/firefoxview/fxview-search-textbox.css" />
    <div class="search-container">
      <div class="search-icon"></div>
      <input
        type="search"
        .placeholder=${ifDefined(this.placeholder)}
        .value=${this.query}
        @input=${this.onInput}
      ></input>
      <div
        class="clear-icon"
        ?hidden=${!this.query}
        @click=${this.clear}
        data-l10n-id="firefoxview-search-text-box-clear-button"
      ></div>
    </div>`;
  }
}

customElements.define("fxview-search-textbox", FxviewSearchTextbox);
