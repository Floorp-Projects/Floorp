/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ShoppingProduct } from "chrome://global/content/shopping/ShoppingProduct.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html } from "chrome://global/content/vendor/lit.all.mjs";

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
    return html` <p>${JSON.stringify(this.data)}</p> `;
  }
}

customElements.define("shopping-container", ShoppingContainer);
