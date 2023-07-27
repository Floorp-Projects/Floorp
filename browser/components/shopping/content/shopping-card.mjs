/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * A card container to be used in the shopping sidebar. There are three card types.
 * The default type where no type attribute is required and the card will have no extra functionality.
 * The "accordion" type will initially not show any content. The card will contain a arrow to expand the
 * card so all of the content is visible.
 *
 * @property {string} label - The label text that will be used for the card header
 * @property {string} type - (optional) The type of card. No type specified
 *   will be the default card. The other available type is "accordion".
 */
class ShoppingCard extends MozLitElement {
  static properties = {
    label: { type: String },
    type: { type: String },
  };

  static get queries() {
    return {
      detailsEl: "#shopping-details",
    };
  }

  labelTemplate() {
    if (this.label) {
      if (this.type === "accordion") {
        return html`
          <div id="label-wrapper">
            <span id="header">${this.label}</span>
            <button
              tabindex="-1"
              class="icon chevron-icon ghost-button"
              @click=${this.handleChevronButtonClick}
            ></button>
          </div>
        `;
      }
      return html`
        <div id="label-wrapper">
          <span id="header">${this.label}</span><slot name="rating"></slot>
        </div>
      `;
    }
    return "";
  }

  cardTemplate() {
    if (this.type === "accordion") {
      return html`
        <details id="shopping-details">
          <summary>${this.labelTemplate()}</summary>
          <div id="content"><slot name="content"></slot></div>
        </details>
      `;
    }
    return html`
      ${this.labelTemplate()}
      <div id="content" aria-describedby="content">
        <slot name="content"></slot>
      </div>
    `;
  }

  handleChevronButtonClick() {
    this.detailsEl.open = !this.detailsEl.open;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-card.css"
      />
      <article
        class="shopping-card"
        aria-labelledby="header"
        aria-label=${ifDefined(this.label)}
      >
        ${this.cardTemplate()}
      </article>
    `;
  }
}
customElements.define("shopping-card", ShoppingCard);
