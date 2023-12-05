/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, styleMap } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-message-bar.mjs";

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
    ["thank-you-for-feedback", () => this.thankYouForFeedbackTemplate()],
  ]);

  static properties = {
    type: { type: String },
    productUrl: { type: String, reflect: true },
    progress: { type: Number, reflect: true },
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
    Glean.shopping.surfaceReanalyzeClicked.record();
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
    return html`<message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <span
          data-l10n-id="shopping-message-bar-warning-stale-analysis-message-2"
        ></span>
        <button
          id="message-bar-reanalysis-button"
          class="small-button shopping-button"
          data-l10n-id="shopping-message-bar-warning-stale-analysis-button"
          @click=${this.onClickAnalysisButton}
        ></button>
      </article>
    </message-bar>`;
  }

  getGenericErrorTemplate() {
    return html`<moz-message-bar
      data-l10n-attrs="heading, message"
      type="warning"
      data-l10n-id="shopping-message-bar-generic-error"
    >
    </moz-message-bar>`;
  }

  getNotEnoughReviewsTemplate() {
    return html`<moz-message-bar
      data-l10n-attrs="heading, message"
      type="warning"
      data-l10n-id="shopping-message-bar-warning-not-enough-reviews"
    >
    </moz-message-bar>`;
  }

  getProductNotAvailableTemplate() {
    return html`<moz-message-bar
      data-l10n-attrs="heading, message"
      type="warning"
      data-l10n-id="shopping-message-bar-warning-product-not-available"
    >
      <button
        slot="actions"
        id="message-bar-report-product-available-btn"
        class="small-button shopping-button"
        data-l10n-id="shopping-message-bar-warning-product-not-available-button2"
        @click=${this.onClickProductAvailable}
      ></button>
    </moz-message-bar>`;
  }

  getThanksForReportingTemplate() {
    return html`<moz-message-bar
      data-l10n-attrs="heading, message"
      type="info"
      data-l10n-id="shopping-message-bar-thanks-for-reporting"
    >
    </moz-message-bar>`;
  }

  getProductNotAvailableReportedTemplate() {
    return html`<moz-message-bar
      data-l10n-attrs="heading, message"
      type="warning"
      data-l10n-id="shopping-message-bar-warning-product-not-available-reported"
    >
    </moz-message-bar>`;
  }

  analysisInProgressTemplate() {
    return html`<message-bar
      style=${styleMap({
        "--analysis-progress-pcent": `${this.progress}%`,
      })}
    >
      <article
        id="message-bar-container"
        aria-labelledby="header"
        type="analysis"
      >
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-analysis-in-progress-with-amount"
          data-l10n-args="${JSON.stringify({
            percentage: Math.round(this.progress),
          })}"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-analysis-in-progress-message2"
        ></span>
      </article>
    </message-bar>`;
  }

  reanalysisInProgressTemplate() {
    return html`<message-bar
      style=${styleMap({
        "--analysis-progress-pcent": `${this.progress}%`,
      })}
    >
      <article
        id="message-bar-container"
        aria-labelledby="header"
        type="re-analysis"
      >
        <span
          id="header"
          data-l10n-id="shopping-message-bar-analysis-in-progress-with-amount"
          data-l10n-args="${JSON.stringify({
            percentage: Math.round(this.progress),
          })}"
        ></span>
      </article>
    </message-bar>`;
  }

  pageNotSupportedTemplate() {
    return html`<moz-message-bar
      data-l10n-attrs="heading, message"
      type="warning"
      data-l10n-id="shopping-message-bar-page-not-supported"
    >
    </moz-message-bar>`;
  }

  thankYouForFeedbackTemplate() {
    return html`<moz-message-bar
      data-l10n-attrs="heading"
      type="success"
      dismissable
      data-l10n-id="shopping-survey-thanks"
    >
    </moz-message-bar>`;
  }

  render() {
    let messageBarTemplate = this.#MESSAGE_TYPES_RENDER_TEMPLATE_MAPPING.get(
      this.type
    )();
    if (messageBarTemplate) {
      if (this.type == "stale") {
        Glean.shopping.surfaceStaleAnalysisShown.record();
      }
      return html`
        <link
          rel="stylesheet"
          href="chrome://browser/content/shopping/shopping-message-bar.css"
        />
        <link
          rel="stylesheet"
          href="chrome://browser/content/shopping/shopping-page.css"
        />
        ${messageBarTemplate}
      `;
    }
    return null;
  }
}

customElements.define("shopping-message-bar", ShoppingMessageBar);
