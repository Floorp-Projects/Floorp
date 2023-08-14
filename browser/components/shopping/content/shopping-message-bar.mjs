/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

class ShoppingMessageBar extends MozLitElement {
  #MESSAGE_TYPES_RENDER_TEMPLATE_MAPPING = new Map([
    ["stale", this.getStaleWarningTemplate()],
    ["generic-error", this.getGenericErrorTemplate()],
    ["not-enough-reviews", this.getNotEnoughReviewsTemplate()],
    ["product-not-available", this.getProductNotAvailableTemplate()],
    ["offline", this.getOfflineWarningTemplate()],
    ["analysis-in-progress", this.getAnalysisInProgressTemplate()],
  ]);

  static properties = {
    type: { type: String },
  };

  static get queries() {
    return {
      reAnalysisLinkEl: "#message-bar-reanalysis-link",
    };
  }

  onClickAnalysisLink() {
    this.dispatchEvent(
      new CustomEvent("ReAnalysisRequested", {
        bubbles: true,
        composed: true,
      })
    );
  }

  getStaleWarningTemplate() {
    // TODO: Bug 1843142 - add proper stale analysis link once finalized
    return html` <message-bar type="warning">
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-stale-analysis-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-stale-analysis-message"
        ></span>
        <a
          id="message-bar-reanalysis-link"
          target="_blank"
          data-l10n-id="shopping-message-bar-warning-stale-analysis-link"
          href="#"
          @click=${this.onClickAnalysisLink}
        ></a>
      </article>
    </message-bar>`;
  }

  getGenericErrorTemplate() {
    return html` <message-bar type="error">
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-generic-error-title"
        ></strong>
        <span data-l10n-id="shopping-message-bar-generic-error-message"></span>
      </article>
    </message-bar>`;
  }

  getNotEnoughReviewsTemplate() {
    return html` <message-bar type="warning">
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-not-enough-reviews-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-not-enough-reviews-message"
        ></span>
      </article>
    </message-bar>`;
  }

  getProductNotAvailableTemplate() {
    return html`<message-bar type="warning">
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-product-not-available-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-product-not-available-message"
        ></span>
        <button
          class="small-button"
          data-l10n-id="shopping-message-bar-warning-product-not-available-button"
        ></button>
      </article>
    </message-bar>`;
  }
  getAnalysisInProgressTemplate() {
    // TODO: Bug 1847839 - insert spinner into message-bar
    return html` <message-bar>
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-analysis-in-progress-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-analysis-in-progress-message"
        ></span>
      </article>
    </message-bar>`;
  }

  getOfflineWarningTemplate() {
    return html` <message-bar type="warning">
      <article id="message-bar-container" aria-labelledby="header">
        <strong
          id="header"
          data-l10n-id="shopping-message-bar-warning-offline-title"
        ></strong>
        <span
          data-l10n-id="shopping-message-bar-warning-offline-message"
        ></span>
      </article>
    </message-bar>`;
  }

  render() {
    let messageBarTemplate = this.#MESSAGE_TYPES_RENDER_TEMPLATE_MAPPING.get(
      this.type
    );
    if (messageBarTemplate) {
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

  updated() {
    // message-bar does not support adding a header and does not align it with the icon.
    // Override styling to make them align.
    let messageBar = this.renderRoot.querySelector("message-bar");
    let messageBarContainer = messageBar.shadowRoot.querySelector(".container");
    let icon = messageBarContainer.querySelector(".icon");

    messageBarContainer.style.alignItems = "start";
    messageBarContainer.style.padding = "0.5rem 0.75rem";
    messageBarContainer.style.gap = "0.75rem";
    icon.style.paddingBlockStart = "0";
  }
}

customElements.define("shopping-message-bar", ShoppingMessageBar);
