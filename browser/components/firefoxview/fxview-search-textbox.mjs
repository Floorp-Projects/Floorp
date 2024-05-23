/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const SEARCH_DEBOUNCE_RATE_MS = 500;

/**
 * A search box that displays a search icon and is clearable. Updates to the
 * search query trigger a `fxview-search-textbox-query` event with the current
 * query value.
 *
 * There is no actual searching done here. That needs to be implemented by the
 * `fxview-search-textbox-query` event handler. `searchTabList()` from
 * `search-helpers.mjs` can be used as a starting point.
 *
 * @property {string} placeholder
 *   The placeholder text for the search box.
 * @property {number} size
 *   The width (number of characters) of the search box.
 * @property {string} pageName
 *   The hash for the page name that the search input is located on.
 */
export default class FxviewSearchTextbox extends MozLitElement {
  static properties = {
    placeholder: { type: String },
    size: { type: Number },
    pageName: { type: String },
  };

  static queries = {
    clearButton: ".clear-icon",
    input: "input",
  };

  #query = "";
  #searchTimer;

  disconnectedCallback() {
    super.disconnectedCallback();
    clearTimeout(this.#searchTimer);
  }

  focus() {
    this.input.focus();
  }

  blur() {
    this.input.blur();
  }

  onInput(event) {
    this.#query = event.target.value.trim();
    event.preventDefault();
    this.onSearch();
  }

  /**
   * Handler for query updates from keyboard input, and textbox clears from 'X'
   * button.
   */
  onSearch() {
    clearTimeout(this.#searchTimer);
    this.#searchTimer = setTimeout(
      () => this.#dispatchQueryEvent(),
      SEARCH_DEBOUNCE_RATE_MS
    );
    this.requestUpdate();
  }

  clear(event) {
    if (
      event.type == "click" ||
      (event.type == "keydown" && event.code == "Enter") ||
      (event.type == "keydown" && event.code == "Space")
    ) {
      this.#query = "";
      event.preventDefault();
      this.onSearch();
    }
  }

  #dispatchQueryEvent() {
    window.scrollTo(0, 0);
    this.dispatchEvent(
      new CustomEvent("fxview-search-textbox-query", {
        bubbles: true,
        composed: true,
        detail: { query: this.#query },
      })
    );
    if (!window.IS_STORYBOOK) {
      Services.telemetry.recordEvent(
        "firefoxview_next",
        "search_initiated",
        "search",
        null,
        {
          page: this.pageName,
        }
      );
    }
  }

  render() {
    return html`
    <link rel="stylesheet" href="chrome://browser/content/firefoxview/fxview-search-textbox.css" />
    <div class="search-container">
      <div class="search-icon"></div>
      <input
        type="search"
        .placeholder=${ifDefined(this.placeholder)}
        .size=${ifDefined(this.size)}
        .value=${this.#query}
        @input=${this.onInput}
      ></input>
      <div
        class="clear-icon"
        role="button"
        tabindex="0"
        ?hidden=${!this.#query}
        @click=${this.clear}
        @keydown=${this.clear}
        data-l10n-id="firefoxview-search-text-box-clear-button"
      ></div>
    </div>`;
  }
}

customElements.define("fxview-search-textbox", FxviewSearchTextbox);
