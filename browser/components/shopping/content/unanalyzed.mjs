/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/shopping-card.mjs";

class UnanalyzedProductCard extends MozLitElement {
  static properties = {
    productURL: { type: String, reflect: true },
  };

  // TODO: add link for product analysis once finalized
  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/unanalyzed.css"
      />
      <shopping-card>
        <div id="unanalyzed-product-wrapper" slot="content">
          <span id="unanalyzed-product-icon"></span>
          <div id="unanalyzed-product-message-content">
            <strong data-l10n-id="shopping-unanalyzed-product-header"></strong>
            <p data-l10n-id="shopping-unanalyzed-product-message"></p>
          </div>
          <a
            id="unanalyzed-product-analysis-link"
            data-l10n-id="shopping-unanalyzed-product-analyze-link"
            target="_blank"
            href="https://staging.fakespot.com/analyze?url=${this.productURL}"
          ></a>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("unanalyzed-product-card", UnanalyzedProductCard);
