/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const VALID_LETTER_GRADE_L10N_IDS = new Map([
  ["A", "shopping-letter-grade-description-ab"],
  ["B", "shopping-letter-grade-description-ab"],
  ["C", "shopping-letter-grade-description-c"],
  ["D", "shopping-letter-grade-description-df"],
  ["F", "shopping-letter-grade-description-df"],
]);

class LetterGrade extends MozLitElement {
  #descriptionL10N;

  static properties = {
    letter: { type: String, reflect: true },
    showdescription: { type: Boolean, reflect: true },
  };

  get fluentStrings() {
    if (!this._fluentStrings) {
      this._fluentStrings = new Localization(["browser/shopping.ftl"], true);
    }
    return this._fluentStrings;
  }

  descriptionTemplate() {
    if (this.showdescription) {
      return html`<p
        id="letter-grade-description"
        data-l10n-id=${this.#descriptionL10N}
      ></p>`;
    }

    return null;
  }

  render() {
    // Do not render if letter is invalid
    if (!VALID_LETTER_GRADE_L10N_IDS.has(this.letter)) {
      return null;
    }

    this.#descriptionL10N = VALID_LETTER_GRADE_L10N_IDS.get(this.letter);
    let tooltipL10NArgs;
    // Do not localize tooltip if using Storybook.
    if (!window.IS_STORYBOOK) {
      const localizedDescription = this.fluentStrings.formatValueSync(
        this.#descriptionL10N
      );
      tooltipL10NArgs = `{"letter": "${this.letter}", "description": "${localizedDescription}"}`;
    }

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/letter-grade.css"
      />
      <article
        id="letter-grade-wrapper"
        data-l10n-id="shopping-letter-grade-tooltip"
        data-l10n-attrs="title"
        data-l10n-args=${tooltipL10NArgs}
      >
        <p id="letter-grade-term">${this.letter}</p>
        ${this.descriptionTemplate()}
      </article>
    `;
  }
}

customElements.define("letter-grade", LetterGrade);
