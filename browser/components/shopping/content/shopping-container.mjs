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

export class ShoppingContainer extends MozLitElement {
  static properties = {
    data: { type: Object },
  };

  connectedCallback() {
    super.connectedCallback();
    if (!this.data) {
      this.fetchAnalysis();
    }
  }

  async fetchAnalysis() {
    let searchParams = new URLSearchParams(window.location.search);
    let productPage = searchParams.get("url");
    if (!productPage) {
      // Nothing to do - we're navigating to a non-product page.
      return;
    }
    let productURL = new URL(productPage);
    let product = new ShoppingProduct(productURL);
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
          <review-reliability letter=${this.data.grade}></review-reliability>
          <adjusted-rating
            rating=${this.data.adjusted_rating}
          ></adjusted-rating>
          <review-highlights></review-highlights>
          <shopping-settings></shopping-settings>
        </div>
      </div>`;
  }
}

customElements.define("shopping-container", ShoppingContainer);
