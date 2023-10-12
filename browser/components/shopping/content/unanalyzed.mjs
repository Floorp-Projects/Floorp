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

  static get queries() {
    return {
      analysisButtonEl: "#unanalyzed-product-analysis-button",
    };
  }

  onClickAnalysisButton() {
    this.dispatchEvent(
      new CustomEvent("NewAnalysisRequested", {
        bubbles: true,
        composed: true,
      })
    );
    Glean.shopping.surfaceAnalyzeReviewsNoneAvailableClicked.record();
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/unanalyzed.css"
      />
      <shopping-card>
        <div id="unanalyzed-product-wrapper" slot="content">
          <img id="unanalyzed-product-icon" role="presentation" alt=""></img>
          <div id="unanalyzed-product-message-content">
            <h2
              data-l10n-id="shopping-unanalyzed-product-header-2"
            ></h2>
            <p data-l10n-id="shopping-unanalyzed-product-message-2"></p>
          </div>
          <button
            id="unanalyzed-product-analysis-button"
            class="primary"
            data-l10n-id="shopping-unanalyzed-product-analyze-button"
            @click=${this.onClickAnalysisButton}
          ></button>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("unanalyzed-product-card", UnanalyzedProductCard);
