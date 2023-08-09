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
      ratingEl: "moz-five-star",
      linkEl: "#ad-title",
    };
  }

  disconnectedCallback() {
    this.clearRecommendationAdTimeout();
  }

  clearRecommendationAdTimeout() {
    if (this.recommendationAdTimeout) {
      clearTimeout(this.recommendationAdTimeout);
    }
  }

  adImpression() {
    // TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=1846774
    // We want to send an api call when the ad is view for 1 second
    // this.dispatchEvent(
    //   new CustomEvent("Shopping:AdImpression", {
    //     bubbles: true,
    //   })
    // );

    this.clearRecommendationAdTimeout();
  }

  handleClick(event) {
    event.preventDefault();
    window.open(this.product.url, "_blank");

    // TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=1846774
    // We want to send an api call when the ad is clicked
    // this.dispatchEvent(
    //   new CustomEvent("Shopping:AdClicked", {
    //     bubbles: true,
    //   })
    // );
  }

  priceTemplate() {
    // We are only showing prices in USD for now. In the future we will need
    // to update this to include other currencies.
    return html`<span id="price">$${this.product.price}</span>`;
  }

  render() {
    if (!this.adSeen) {
      this.recommendationAdTimeout = setTimeout(
        () => this.adImpression(),
        AD_IMPRESSION_TIMEOUT
      );
      this.adSeen = true;
    }

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/recommended-ad.css"
      />
      <shopping-card
        data-l10n-id="recommended-by-fakespot-ad-label"
        data-l10n-attrs="label"
      >
        <a id="recommended-ad-wrapper" slot="content" href=${
          this.product.url
        } target="_blank" @click=${this.handleClick}>
          <div id="ad-content">
            <img id="ad-preview-image" src=${this.product.image_url}></img>
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
    `;
  }
}

customElements.define("recommended-ad", RecommendedAd);
