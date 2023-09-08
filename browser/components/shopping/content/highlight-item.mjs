/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * Class for displaying a list of highlighted product reviews, according to highlight category.
 */
class Highlight extends MozLitElement {
  l10nId;
  highlightType;
  /**
   * reviews is a list of Strings, representing all the reviews to display
   * under a highlight category.
   */
  reviews;

  /**
   * lang defines the language in which the reviews are written. We should specify
   * language so that screen readers can read text with the appropriate language packs.
   */
  lang;

  render() {
    let ulTemplate = [];

    for (let review of this.reviews) {
      ulTemplate.push(
        html`<li>
          <q><span lang=${this.lang}>${review}</span></q>
        </li>`
      );
    }

    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/highlight-item.css"
      />
      <div class="highlight-item-wrapper">
        <span class="highlight-icon ${this.highlightType}"></span>
        <dt class="highlight-label" data-l10n-id=${this.l10nId}></dt>
        <dd class="highlight-details">
          <ul class="highlight-details-list">
            ${ulTemplate}
          </ul>
        </dd>
      </div>
    `;
  }
}

customElements.define("highlight-item", Highlight);
