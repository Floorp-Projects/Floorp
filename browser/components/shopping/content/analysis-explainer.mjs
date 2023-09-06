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
  getGradesDescriptionTemplate() {
    return html`
      <section id="analysis-explainer-grades-wrapper">
        <p data-l10n-id="shopping-analysis-explainer-grades-intro"></p>
        <ul id="analysis-explainer-grades-list">
          <li
            data-l10n-id="shopping-analysis-explainer-higher-grade-description"
          ></li>
          <li
            data-l10n-id="shopping-analysis-explainer-lower-grade-description"
          ></li>
        </ul>
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
        <p
          id="analysis-explainer-grading-scale-header"
          data-l10n-id="shopping-analysis-explainer-review-grading-scale"
        ></p>
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
            <p data-l10n-id="shopping-analysis-explainer-intro"></p>
            ${this.getGradesDescriptionTemplate()}
            <p
              data-l10n-id="shopping-analysis-explainer-adjusted-rating-description"
            ></p>
            <p
              data-l10n-id="shopping-analysis-explainer-highlights-description"
            ></p>
            <p data-l10n-id="shopping-analysis-explainer-learn-more">
              <a
                is="moz-support-link"
                support-page="todo"
                data-l10n-name="review-quality-url"
              ></a>
            </p>
            ${this.getGradingScaleListTemplate()}
          </div>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("analysis-explainer", AnalysisExplainer);
