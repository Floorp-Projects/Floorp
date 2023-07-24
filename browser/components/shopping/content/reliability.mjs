/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/letter-grade.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/shopping-card.mjs";

class ReviewReliability extends MozLitElement {
  static properties = {
    letter: { type: String },
  };

  static get queries() {
    return {
      letterGradeEl: "letter-grade",
    };
  }

  render() {
    if (!this.letter) {
      this.hidden = true;
      return null;
    }

    return html`
      <shopping-card
        data-l10n-id="shopping-review-reliability-label"
        data-l10n-attrs="label"
      >
        <div slot="content">
          <letter-grade
            letter=${ifDefined(this.letter)}
            showdescription
          ></letter-grade>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("review-reliability", ReviewReliability);
