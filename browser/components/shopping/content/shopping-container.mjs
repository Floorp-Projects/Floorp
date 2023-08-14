/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";

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
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/unanalyzed.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/recommended-ad.mjs";

export class ShoppingContainer extends MozLitElement {
  static properties = {
    data: { type: Object },
    showOnboarding: { type: Boolean },
    productUrl: { type: String },
    recommendationData: { type: Object },
    isOffline: { type: Boolean },
    analysisEvent: { type: Object },
  };

  static get queries() {
    return {
      reviewReliabilityEl: "review-reliability",
      adjustedRatingEl: "adjusted-rating",
      highlightsEl: "review-highlights",
      settingsEl: "shopping-settings",
      analysisExplainerEl: "analysis-explainer",
      unanalyzedProductEl: "unanalyzed-product-card",
      shoppingMessageBarEl: "shopping-message-bar",
    };
  }

  connectedCallback() {
    super.connectedCallback();
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    window.document.addEventListener("Update", this);
    window.document.addEventListener("NewAnalysisRequested", this);
    window.document.addEventListener("ReAnalysisRequested", this);

    window.dispatchEvent(
      new CustomEvent("ContentReady", {
        bubbles: true,
        composed: true,
      })
    );
  }

  async _update({
    data,
    showOnboarding,
    productUrl,
    recommendationData,
    isPolledRequestDone,
  }) {
    // If we're not opted in or there's no shopping URL in the main browser,
    // the actor will pass `null`, which means this will clear out any existing
    // content in the sidebar.
    this.data = data;
    this.showOnboarding = showOnboarding;
    this.productUrl = productUrl;
    this.recommendationData = recommendationData;
    this.isOffline = !navigator.onLine;
    this.isPolledRequestDone = isPolledRequestDone;
  }

  handleEvent(event) {
    switch (event.type) {
      case "Update":
        this._update(event.detail);
        break;
      case "NewAnalysisRequested":
      case "ReAnalysisRequested":
        this.analysisEvent = {
          type: event.type,
          productUrl: this.productUrl,
        };
        window.dispatchEvent(
          new CustomEvent("PolledRequestMade", {
            bubbles: true,
            composed: true,
          })
        );
        break;
    }
  }

  getAnalysisDetailsTemplate() {
    return html`
      <review-reliability letter=${this.data.grade}></review-reliability>
      <adjusted-rating rating=${this.data.adjusted_rating}></adjusted-rating>
      <review-highlights
        .highlights=${this.data.highlights}
      ></review-highlights>
      <analysis-explainer></analysis-explainer>
      ${this.recommendationTemplate()}
    `;
  }

  getContentTemplate() {
    // The user requested an analysis which is not done yet.
    // We only want to show the analysis-in-progress message-bar
    // for the product currently in view.
    if (
      this.analysisEvent?.productUrl == this.productUrl &&
      !this.isPolledRequestDone
    ) {
      return html`<shopping-message-bar
          type="analysis-in-progress"
        ></shopping-message-bar>
        ${this.analysisEvent.type == "ReAnalysisRequested"
          ? this.getAnalysisDetailsTemplate()
          : null}`;
    }

    if (this.data?.error) {
      return html`<shopping-message-bar
        type="generic-error"
      ></shopping-message-bar>`;
    }

    if (this.data.deleted_product) {
      return html`<shopping-message-bar
        type="product-not-available"
      ></shopping-message-bar>`;
    }

    if (this.data.needs_analysis) {
      if (!this.data.product_id) {
        // Product is not yet registered to our db and thus we cannot show any data.
        return html`<unanalyzed-product-card
          productUrl=${ifDefined(this.productUrl)}
        ></unanalyzed-product-card>`;
      }

      if (!this.data.grade || !this.data.adjusted_rating) {
        // We already saw and tried to analyze this product before, but there are not enough reviews
        // to make a detailed analysis.
        return html`<shopping-message-bar
          type="not-enough-reviews"
        ></shopping-message-bar>`;
      }
      // We successfully analyzed the product before, but the current analysis is outdated and can be updated
      // via a re-analysis.
      return html`
        <shopping-message-bar type="stale"></shopping-message-bar>
        ${this.getAnalysisDetailsTemplate()}
      `;
    }

    return this.getAnalysisDetailsTemplate();
  }

  recommendationTemplate() {
    if (this.recommendationData?.length) {
      return html`<recommended-ad
        .product=${this.recommendationData[0]}
      ></recommended-ad>`;
    }
    return null;
  }

  getLoadingTemplate() {
    /* Due to limitations with aria-busy for certain screen readers
     * (see Bug 1682063), mark loading container as a pseudo image and
     * use aria-label as a workaround. */
    return html`
      <div id="loading-wrapper" data-l10n-id="shopping-a11y-loading" role="img">
        <div class="loading-box medium"></div>
        <div class="loading-box medium"></div>
        <div class="loading-box large"></div>
        <div class="loading-box small"></div>
        <div class="loading-box large"></div>
        <div class="loading-box small"></div>
      </div>
    `;
  }

  renderContainer(sidebarContent, hideSettings = false) {
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
        <div id="content" aria-busy=${!this.data}>
          ${sidebarContent}
          ${!hideSettings
            ? html`<shopping-settings></shopping-settings>`
            : null}
        </div>
      </div>`;
  }

  render() {
    let content;
    let hideSettings;
    if (this.showOnboarding) {
      content = html`<slot name="multi-stage-message-slot"></slot>`;
      hideSettings = true;
    } else if (this.isOffline) {
      content = html`<shopping-message-bar
        type="offline"
      ></shopping-message-bar>`;
    } else if (!this.data) {
      content = this.getLoadingTemplate();
      hideSettings = true;
    } else {
      content = this.getContentTemplate();
    }
    return this.renderContainer(content, hideSettings);
  }
}

customElements.define("shopping-container", ShoppingContainer);
