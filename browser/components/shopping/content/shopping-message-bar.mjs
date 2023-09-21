/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

class ShoppingMessageBar extends MozLitElement {
  #MESSAGE_TYPES_RENDER_TEMPLATE_MAPPING = new Map([
    ["stale", () => this.getStaleWarningTemplate()],
    ["generic-error", () => this.getGenericErrorTemplate()],
    ["not-enough-reviews", () => this.getNotEnoughReviewsTemplate()],
    ["product-not-available", () => this.getProductNotAvailableTemplate()],
    ["thanks-for-reporting", () => this.getThanksForReportingTemplate()],
    [
      "product-not-available-reported",
      () => this.getProductNotAvailableReportedTemplate(),
    ],
    ["analysis-in-progress", () => this.analysisInProgressTemplate()],
    ["reanalysis-in-progress", () => this.reanalysisInProgressTemplate()],
    ["page-not-supported", () => this.pageNotSupportedTemplate()],
  ]);

  static properties = {
    type: { type: String },
    productUrl: { type: String, reflect: true },
  };

  static get queries() {
    return {
      reAnalysisButtonEl: "#message-bar-reanalysis-button",
      productAvailableBtnEl: "#message-bar-report-product-available-btn",
    };
  }

  onClickAnalysisButton() {
    this.dispatchEvent(
      new CustomEvent("ReanalysisRequested", {
        bubbles: true,
        composed: true,
      })
    );
    this.dispatchEvent(
      new CustomEvent("ShoppingTelemetryEvent", {
        bubbles: true,
        composed: true,
        detail: "reanalyzeClicked",
      })
    );
  }

  onClickProductAvailable() {
    this.dispatchEvent(
      new CustomEvent("ReportedProductAvailable", {
        bubbles: true,
        composed: true,
      })
    );
  }

  getStaleWarningTemplate() {
    return html` <message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <span
          data-l10n-id="shopping-message-bar-warning-stale-analysis-message-2"
        ></span>
        <button
          id="message-bar-reanalysis-button"
          class="small-button"
          data-l10n-id="shopping-message-bar-warning-stale-analysis-button"
          @click=${this.onClickAnalysisButton}
        ></button>
      </article>
    </message-bar>`;
  }

  getGenericErrorTemplate() {
    return html`<message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-generic-error-title2"
        ></strong>
        <span data-l10n-id="shopping-message-bar-generic-error-message"></span>
      </article>
    </message-bar>`;
  }

  getNotEnoughReviewsTemplate() {
    return html`<message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-not-enough-reviews-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-not-enough-reviews-message2"
        ></span>
      </article>
    </message-bar>`;
  }

  getProductNotAvailableTemplate() {
    return html`<message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-product-not-available-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-product-not-available-message2"
        ></span>
        <button
          id="message-bar-report-product-available-btn"
          class="small-button"
          data-l10n-id="shopping-message-bar-warning-product-not-available-button"
          @click=${this.onClickProductAvailable}
        ></button>
      </article>
    </message-bar>`;
  }

  getThanksForReportingTemplate() {
    return html` <message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-thanks-for-reporting-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-thanks-for-reporting-message2"
        ></span>
      </article>
    </message-bar>`;
  }

  getProductNotAvailableReportedTemplate() {
    return html`<message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-product-not-available-reported-title2"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-product-not-available-reported-message2"
        ></span>
      </article>
    </message-bar>`;
  }

  analysisInProgressTemplate() {
    return html` <message-bar>
      <article
        id="message-bar-container"
        aria-labelledby="header"
        type="analysis"
      >
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-analysis-in-progress-title2"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-analysis-in-progress-message2"
        ></span>
      </article>
    </message-bar>`;
  }

  reanalysisInProgressTemplate() {
    return html` <message-bar>
      <article
        id="message-bar-container"
        aria-labelledby="header"
        type="re-analysis"
      >
        <span
          id="header"
          data-l10n-id="shopping-message-bar-analysis-in-progress-title2"
        ></span>
      </article>
    </message-bar>`;
  }

  pageNotSupportedTemplate() {
    return html` <message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-page-not-supported-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-page-not-supported-message"
        ></span>
      </article>
    </message-bar>`;
  }

  render() {
    let messageBarTemplate = this.#MESSAGE_TYPES_RENDER_TEMPLATE_MAPPING.get(
      this.type
    )();
    if (messageBarTemplate) {
      if (this.type == "stale") {
        this.dispatchEvent(
          new CustomEvent("ShoppingTelemetryEvent", {
            bubbles: true,
            composed: true,
            detail: "staleAnalysisShown",
          })
        );
      }
      return html`
        <link
          rel="stylesheet"
          href="chrome://browser/content/shopping/shopping-message-bar.css"
        />
        ${messageBarTemplate}
      `;
    }
    return null;
  }
}

customElements.define("shopping-message-bar", ShoppingMessageBar);
