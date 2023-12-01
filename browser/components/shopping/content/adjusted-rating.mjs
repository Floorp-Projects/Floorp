/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-five-star.mjs";

/**
 * Class for displaying the adjusted ratings for a given product.
 */
class AdjustedRating extends MozLitElement {
  static properties = {
    rating: { type: Number, reflect: true },
  };

  static get queries() {
    return {
      ratingEl: "moz-five-star",
    };
  }

  render() {
    if (!this.rating && this.rating !== 0) {
      this.hidden = true;
      return null;
    }

    this.hidden = false;

    return html`
      <shopping-card
        data-l10n-id="shopping-adjusted-rating-label"
        data-l10n-attrs="label"
      >
        <moz-five-star
          slot="rating"
          rating="${this.rating === 0 ? 0.5 : this.rating}"
        ></moz-five-star>
        <div slot="content">
          <span
            data-l10n-id="shopping-adjusted-rating-unreliable-reviews"
          ></span>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("adjusted-rating", AdjustedRating);
