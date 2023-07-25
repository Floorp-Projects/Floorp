/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ShoppingProduct } from "chrome://global/content/shopping/ShoppingProduct.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/highlights.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/settings.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/adjusted-rating.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/reliability.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/analysis-explainer.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/shopping-message-bar.mjs";

export class ShoppingContainer extends MozLitElement {
  #productURL;

  static properties = {
    data: { type: Object },
  };

  static get queries() {
    return {
      reviewReliabilityEl: "review-reliability",
      adjustedRatingEl: "adjusted-rating",
      highlightsEl: "review-highlights",
      settingsEl: "shopping-settings",
    };
  }

  connectedCallback() {
    super.connectedCallback();
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    window.document.addEventListener("UpdateProductURL", this);

    window.dispatchEvent(
      new CustomEvent("ContentReady", {
        bubbles: true,
        composed: true,
      })
    );
  }

  set productURL(newURL) {
    this.#productURL = newURL;
    this.data = null;
  }

  get productURL() {
    return this.#productURL;
  }

  init() {
    if (!this.productURL) {
      // TODO: cancel fetches? disconnect components?
    } else if (!this.data) {
      this.fetchAnalysis();
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "UpdateProductURL":
        // Ignore duplicate events sometimes fired by product pages.
        if (this.productURL === event.detail.url) {
          return;
        }
        this.productURL = event.detail.url;
        this.init();
        break;
    }
  }

  async fetchAnalysis() {
    let product = new ShoppingProduct(new URL(this.productURL));
    this.data = await product.requestAnalysis();
  }

  render() {
    if (!this.data) {
      return html`<p>loading...</p>`;
    }

    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-container.css"
      />
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <div id="shopping-container">
        <div id="header-wrapper">
          <div id="shopping-header">
            <span id="shopping-icon"></span>
            <span
              id="header"
              data-l10n-id="shopping-main-container-title"
            ></span>
          </div>
          <button
            id="close-button"
            class="ghost-button"
            data-l10n-id="shopping-close-button"
          ></button>
        </div>
        <div id="content">
          ${this.data.needs_analysis && this.data.product_id
            ? html`<shopping-message-bar type="stale"></shopping-message-bar>`
            : null}
          <review-reliability letter=${this.data.grade}></review-reliability>
          <adjusted-rating
            rating=${this.data.adjusted_rating}
          ></adjusted-rating>
          <review-highlights
            .highlights=${this.data.highlights}
          ></review-highlights>
          <analysis-explainer></analysis-explainer>
          <shopping-settings></shopping-settings>
        </div>
      </div>`;
  }
}

customElements.define("shopping-container", ShoppingContainer);
