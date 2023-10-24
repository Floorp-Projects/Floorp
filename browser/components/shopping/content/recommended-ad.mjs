/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/shopping-card.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-five-star.mjs";

const AD_IMPRESSION_TIMEOUT = 1500;

class RecommendedAd extends MozLitElement {
  static properties = {
    product: { type: Object, reflect: true },
  };

  static get queries() {
    return {
      letterGradeEl: "letter-grade",
      linkEl: "#recommended-ad-wrapper",
      priceEl: "#price",
      ratingEl: "moz-five-star",
    };
  }

  connectedCallback() {
    super.connectedCallback();
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    document.addEventListener("visibilitychange", this);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener("visibilitychange", this);
    this.resetImpressionTimer();
    this.revokeImageUrl();
  }

  startImpressionTimer() {
    if (!this.timeout && document.visibilityState === "visible") {
      this.timeout = setTimeout(
        () => this.recordImpression(),
        AD_IMPRESSION_TIMEOUT
      );
    }
  }

  resetImpressionTimer() {
    this.timeout = clearTimeout(this.timeout);
  }

  revokeImageUrl() {
    if (this.imageUrl) {
      URL.revokeObjectURL(this.imageUrl);
    }
  }

  recordImpression() {
    if (this.hasImpressed) {
      return;
    }

    this.dispatchEvent(
      new CustomEvent("AdImpression", {
        bubbles: true,
        detail: { aid: this.product.aid },
      })
    );

    document.removeEventListener("visibilitychange", this);
    this.resetImpressionTimer();

    this.hasImpressed = true;
  }

  handleClick(event) {
    if (event.button === 0 || event.button === 1) {
      this.dispatchEvent(
        new CustomEvent("AdClicked", {
          bubbles: true,
          detail: { aid: this.product.aid },
        })
      );
    }
  }

  handleEvent(event) {
    if (event.type !== "visibilitychange") {
      return;
    }
    if (document.visibilityState === "visible") {
      this.startImpressionTimer();
    } else if (!this.hasImpressed) {
      this.resetImpressionTimer();
    }
  }

  priceTemplate() {
    // We are only showing prices in USD for now. In the future we will need
    // to update this to include other currencies.
    return html`<span id="price">$${this.product.price}</span>`;
  }

  render() {
    this.startImpressionTimer();

    this.revokeImageUrl();
    this.imageUrl = URL.createObjectURL(this.product.image_blob);

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/recommended-ad.css"
      />
      <shopping-card
        data-l10n-id="more-to-consider-ad-label"
        data-l10n-attrs="label"
      >
        <a id="recommended-ad-wrapper" slot="content" href=${
          this.product.url
        } target="_blank" title="${this.product.name}" @click=${
      this.handleClick
    } @auxclick=${this.handleClick}>
          <div id="ad-content">
            <img id="ad-preview-image" src=${this.imageUrl}></img>
            <span id="ad-title" lang="en">${this.product.name}</span>
            <letter-grade letter="${this.product.grade}"></letter-grade>
          </div>
          <div id="footer">
            ${this.priceTemplate()}
            <moz-five-star rating=${
              this.product.adjusted_rating
            }></moz-five-star>
          </div>
        </a>
      </shopping-card>
      <p data-l10n-id="ad-by-fakespot"></p>
    `;
  }
}

customElements.define("recommended-ad", RecommendedAd);
