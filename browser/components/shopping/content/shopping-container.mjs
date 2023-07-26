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
  #optedIn;
  #product;

  static properties = {
    data: { type: Object },
    showOnboarding: { type: Boolean },
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

    window.document.addEventListener("Update", this);

    window.dispatchEvent(
      new CustomEvent("ContentReady", {
        bubbles: true,
        composed: true,
      })
    );
  }

  async _update({ url, optedIn }) {
    this.#product?.uninit();
    this.#optedIn = optedIn;

    if (this.#optedIn !== 1) {
      this.showOnboarding = true;
      // In case the user just opted out, clear out any product data too.
      this.data = null;
      return;
    }
    this.showOnboarding = false;

    // `url` is null for non-product pages; clear out any sidebar content while
    // the chrome code closes the sidebar.
    if (!url) {
      this.data = null;
      return;
    }

    let product = (this.#product = new ShoppingProduct(new URL(url)));
    let data = await product.requestAnalysis();
    // Double-check that we haven't opted out or re-entered this function
    // while we were `await`-ing.
    if (this.#optedIn !== 1 || product !== this.#product) {
      return;
    }
    this.data = data;
  }

  handleEvent(event) {
    switch (event.type) {
      case "Update":
        this._update(event.detail);
        break;
    }
  }

  renderContainer(sidebarContent) {
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
        <div id="content">${sidebarContent}</div>
      </div>`;
  }

  render() {
    let content;
    if (this.showOnboarding) {
      // For the onboarding case, leave content area blank, so the OMC onboarding
      // card has room to draw itself via react (bug 1839764).
      content = html`<p>Onboarding UI goes here</p>`;
    } else if (!this.data) {
      // TODO: Replace with loading UI component (bug 1840161).
      content = html`<p>Loading UI goes here</p>`;
    } else {
      content = html`
        ${this.data.needs_analysis && this.data.product_id
          ? html`<shopping-message-bar type="stale"></shopping-message-bar>`
          : null}
        <review-reliability letter=${this.data.grade}></review-reliability>
        <adjusted-rating rating=${this.data.adjusted_rating}></adjusted-rating>
        <review-highlights
          .highlights=${this.data.highlights}
        ></review-highlights>
        <analysis-explainer></analysis-explainer>
        <shopping-settings></shopping-settings>
      `;
    }
    return this.renderContainer(content);
  }
}

customElements.define("shopping-container", ShoppingContainer);
