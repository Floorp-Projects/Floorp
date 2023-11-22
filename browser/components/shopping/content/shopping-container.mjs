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

// The number of pixels that must be scrolled from the
// top of the sidebar to show the header box shadow.
const HEADER_SCROLL_PIXEL_OFFSET = 8;

export class ShoppingContainer extends MozLitElement {
  static properties = {
    data: { type: Object },
    showOnboarding: { type: Boolean },
    productUrl: { type: String },
    recommendationData: { type: Array },
    isOffline: { type: Boolean },
    analysisEvent: { type: Object },
    userReportedAvailable: { type: Boolean },
    adsEnabled: { type: Boolean },
    adsEnabledByUser: { type: Boolean },
    isAnalysisInProgress: { type: Boolean },
    analysisProgress: { type: Number },
    isOverflow: { type: Boolean },
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
      recommendedAdEl: "recommended-ad",
      loadingEl: "#loading-wrapper",
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
    window.document.addEventListener("ReanalysisRequested", this);
    window.document.addEventListener("ReportedProductAvailable", this);
    window.document.addEventListener("adsEnabledByUserChanged", this);
    window.document.addEventListener("scroll", this);
    window.document.addEventListener("UpdateRecommendations", this);
    window.document.addEventListener("UpdateAnalysisProgress", this);

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
    adsEnabled,
    adsEnabledByUser,
    isAnalysisInProgress,
    analysisProgress,
  }) {
    // If we're not opted in or there's no shopping URL in the main browser,
    // the actor will pass `null`, which means this will clear out any existing
    // content in the sidebar.
    this.data = data;
    this.showOnboarding = showOnboarding;
    this.productUrl = productUrl;
    this.recommendationData = recommendationData;
    this.isOffline = !navigator.onLine;
    this.isAnalysisInProgress = isAnalysisInProgress;
    this.adsEnabled = adsEnabled;
    this.adsEnabledByUser = adsEnabledByUser;
    this.analysisProgress = analysisProgress;
  }

  _updateRecommendations({ recommendationData }) {
    this.recommendationData = recommendationData;
  }

  _updateAnalysisProgress({ progress }) {
    this.analysisProgress = progress;
  }

  handleEvent(event) {
    switch (event.type) {
      case "Update":
        this._update(event.detail);
        break;
      case "NewAnalysisRequested":
      case "ReanalysisRequested":
        this.isAnalysisInProgress = true;
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
      case "ReportedProductAvailable":
        this.userReportedAvailable = true;
        window.dispatchEvent(
          new CustomEvent("ReportProductAvailable", {
            bubbles: true,
            composed: true,
          })
        );
        Glean.shopping.surfaceReactivatedButtonClicked.record();
        break;
      case "adsEnabledByUserChanged":
        this.adsEnabledByUser = event.detail?.adsEnabledByUser;
        break;
      case "scroll":
        let scrollYPosition = window.scrollY;
        this.isOverflow = scrollYPosition > HEADER_SCROLL_PIXEL_OFFSET;
        break;
      case "UpdateRecommendations":
        this._updateRecommendations(event.detail);
        break;
      case "UpdateAnalysisProgress":
        this._updateAnalysisProgress(event.detail);
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
    `;
  }

  getContentTemplate() {
    // The user requested an analysis which is not done yet.
    if (
      this.analysisEvent?.productUrl == this.productUrl &&
      this.isAnalysisInProgress
    ) {
      const isReanalysis = this.analysisEvent.type === "ReanalysisRequested";
      return html`<shopping-message-bar
          type=${isReanalysis
            ? "reanalysis-in-progress"
            : "analysis-in-progress"}
          progress=${this.analysisProgress}
        ></shopping-message-bar>
        ${isReanalysis ? this.getAnalysisDetailsTemplate() : null}`;
    }

    if (this.data?.error) {
      return html`<shopping-message-bar
        type="generic-error"
      ></shopping-message-bar>`;
    }

    if (this.data.page_not_supported) {
      return html`<shopping-message-bar
        type="page-not-supported"
      ></shopping-message-bar>`;
    }

    if (this.data.deleted_product_reported) {
      return html`<shopping-message-bar
        type="product-not-available-reported"
      ></shopping-message-bar>`;
    }

    if (this.data.deleted_product) {
      return this.userReportedAvailable
        ? html`<shopping-message-bar
            type="thanks-for-reporting"
          ></shopping-message-bar>`
        : html`<shopping-message-bar
            type="product-not-available"
          ></shopping-message-bar>`;
    }

    if (this.data.needs_analysis) {
      if (!this.data.product_id) {
        // Product is new to us.
        return html`<unanalyzed-product-card
          productUrl=${ifDefined(this.productUrl)}
        ></unanalyzed-product-card>`;
      }

      // We successfully analyzed the product before, but the current analysis is outdated and can be updated
      // via a re-analysis.
      return html`
        <shopping-message-bar
          type="stale"
          .productUrl=${this.productUrl}
        ></shopping-message-bar>
        ${this.getAnalysisDetailsTemplate()}
      `;
    }

    if (this.data.not_enough_reviews) {
      // We already saw and tried to analyze this product before, but there are not enough reviews
      // to make a detailed analysis.
      return html`<shopping-message-bar
        type="not-enough-reviews"
      ></shopping-message-bar>`;
    }

    return this.getAnalysisDetailsTemplate();
  }

  recommendationTemplate() {
    const canShowAds = this.adsEnabled && this.adsEnabledByUser;
    if (this.recommendationData?.length && canShowAds) {
      return html`<recommended-ad
        .product=${this.recommendationData[0]}
      ></recommended-ad>`;
    }
    return null;
  }

  /**
   * @param {object?} options
   * @param {boolean?} options.animate = true
   *        Whether to animate the loading state. Defaults to true.
   *        There will be no animation for users who prefer reduced motion,
   *        irrespective of the value of this option.
   */
  getLoadingTemplate({ animate = true } = {}) {
    /* Due to limitations with aria-busy for certain screen readers
     * (see Bug 1682063), mark loading container as a pseudo image and
     * use aria-label as a workaround. */
    return html`
      <div
        id="loading-wrapper"
        data-l10n-id="shopping-a11y-loading"
        role="img"
        class=${animate ? "animate" : ""}
      >
        <div class="loading-box medium"></div>
        <div class="loading-box medium"></div>
        <div class="loading-box large"></div>
        <div class="loading-box small"></div>
        <div class="loading-box large"></div>
        <div class="loading-box small"></div>
      </div>
    `;
  }

  renderContainer(sidebarContent, hideFooter = false) {
    return html`<link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-container.css"
      />
      <link
        rel="stylesheet"
        href="chrome://global/skin/in-content/common.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-page.css"
      />
      <div id="shopping-container">
        <div
          id="header-wrapper"
          class=${this.isOverflow ? "shopping-header-overflow" : ""}
        >
          <header id="shopping-header" data-l10n-id="shopping-a11y-header">
            <h1
              id="shopping-header-title"
              data-l10n-id="shopping-main-container-title"
            ></h1>
            <p id="beta-marker" data-l10n-id="shopping-beta-marker"></p>
          </header>
          <button
            id="close-button"
            class="ghost-button"
            data-l10n-id="shopping-close-button"
            @click=${this.handleClick}
          ></button>
        </div>
        <div id="content" aria-busy=${!this.data}>
          <slot name="multi-stage-message-slot"></slot>
          ${sidebarContent} ${!hideFooter ? this.getFooterTemplate() : null}
        </div>
      </div>`;
  }

  getFooterTemplate() {
    return html`
      <analysis-explainer
        productUrl=${ifDefined(this.productUrl)}
      ></analysis-explainer>
      ${this.recommendationTemplate()}
      <shopping-settings
        ?adsEnabled=${this.adsEnabled}
        ?adsEnabledByUser=${this.adsEnabledByUser}
      ></shopping-settings>
    `;
  }

  render() {
    let content;
    let hideFooter;
    if (this.showOnboarding) {
      content = html``;
      hideFooter = true;
    } else if (this.isOffline) {
      content = this.getLoadingTemplate({ animate: false });
      hideFooter = true;
    } else if (!this.data) {
      if (this.isAnalysisInProgress) {
        content = html`<shopping-message-bar
          type="analysis-in-progress"
          progress=${this.analysisProgress}
        ></shopping-message-bar>`;
      } else {
        content = this.getLoadingTemplate();
        hideFooter = true;
      }
    } else {
      content = this.getContentTemplate();
    }
    return this.renderContainer(content, hideFooter);
  }

  handleClick() {
    RPMSetPref("browser.shopping.experience2023.active", false);
    Glean.shopping.surfaceClosed.record({ source: "closeButton" });
  }
}

customElements.define("shopping-container", ShoppingContainer);
