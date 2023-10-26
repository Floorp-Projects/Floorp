/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/shopping-card.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/letter-grade.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-support-link.mjs";

const VALID_EXPLAINER_L10N_IDS = new Map([
  ["reliable", "shopping-analysis-explainer-review-grading-scale-reliable"],
  ["mixed", "shopping-analysis-explainer-review-grading-scale-mixed"],
  ["unreliable", "shopping-analysis-explainer-review-grading-scale-unreliable"],
]);

/**
 * Class for displaying details about letter grades, adjusted rating, and highlights.
 */
class AnalysisExplainer extends MozLitElement {
  static properties = {
    productUrl: { type: String, reflect: true },
  };

  static get queries() {
    return {
      reviewQualityExplainerLink: "#review-quality-url",
    };
  }

  getGradesDescriptionTemplate() {
    return html`
      <section id="analysis-explainer-grades-wrapper">
        <p data-l10n-id="shopping-analysis-explainer-grades-intro"></p>
      </section>
    `;
  }

  createGradingScaleEntry(letters, descriptionL10nId) {
    let letterGradesTemplate = [];
    for (let letter of letters) {
      letterGradesTemplate.push(
        html`<letter-grade letter=${letter}></letter-grade>`
      );
    }
    return html`
      <div class="analysis-explainer-grading-scale-entry">
        <dt class="analysis-explainer-grading-scale-term">
          <span class="analysis-explainer-grading-scale-letters">
            ${letterGradesTemplate}
          </span>
        </dt>
        <dd
          class="analysis-explainer-grading-scale-description"
          data-l10n-id=${descriptionL10nId}
        ></dd>
      </div>
    `;
  }

  getGradingScaleListTemplate() {
    return html`
      <section id="analysis-explainer-grading-scale-wrapper">
        <dl id="analysis-explainer-grading-scale-list">
          ${this.createGradingScaleEntry(
            ["A", "B"],
            VALID_EXPLAINER_L10N_IDS.get("reliable")
          )}
          ${this.createGradingScaleEntry(
            ["C"],
            VALID_EXPLAINER_L10N_IDS.get("mixed")
          )}
          ${this.createGradingScaleEntry(
            ["D", "F"],
            VALID_EXPLAINER_L10N_IDS.get("unreliable")
          )}
        </dl>
      </section>
    `;
  }

  // It turns out we must always return a non-empty string: if not, the fluent
  // resolver will complain that the variable value is missing. We use the
  // placeholder "retailer", which should never be visible to users.
  getRetailerDisplayName() {
    let defaultName = "retailer";
    if (!this.productUrl) {
      return defaultName;
    }
    let url = new URL(this.productUrl);
    let hostname = url.hostname;
    let displayNames = {
      "www.amazon.com": "Amazon",
      "www.bestbuy.com": "Best Buy",
      "www.walmart.com": "Walmart",
    };
    return displayNames[hostname] ?? defaultName;
  }

  handleReviewQualityUrlClicked(e) {
    if (e.target.localName == "a" && e.button == 0) {
      Glean.shopping.surfaceShowQualityExplainerUrlClicked.record();
    }
  }

  // Bug 1857620: rather than manually set the utm parameters on the SUMO link,
  // we should instead update moz-support-link to allow arbitrary utm parameters.
  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/analysis-explainer.css"
      />
      <shopping-card
        data-l10n-id="shopping-analysis-explainer-label"
        data-l10n-attrs="label"
        type="accordion"
      >
        <div slot="content">
          <div id="analysis-explainer-wrapper">
            <p data-l10n-id="shopping-analysis-explainer-intro2"></p>
            ${this.getGradesDescriptionTemplate()}
            ${this.getGradingScaleListTemplate()}
            <p
              data-l10n-id="shopping-analysis-explainer-adjusted-rating-description"
            ></p>
            <p
              data-l10n-id="shopping-analysis-explainer-highlights-description"
              data-l10n-args="${JSON.stringify({
                retailer: this.getRetailerDisplayName(),
              })}"
            ></p>
            <p
              data-l10n-id="shopping-analysis-explainer-learn-more2"
              @click=${this.handleReviewQualityUrlClicked}
            >
              <a
                id="review-quality-url"
                data-l10n-name="review-quality-url"
                target="_blank"
                href="${window.RPMGetFormatURLPref(
                  "app.support.baseURL"
                )}review-checker-review-quality?as=u&utm_source=inproduct&utm_campaign=learn-more&utm_term=core-sidebar"
              ></a>
            </p>
          </div>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("analysis-explainer", AnalysisExplainer);
