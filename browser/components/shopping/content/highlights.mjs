/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/highlight-item.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/shopping-card.mjs";

const VALID_HIGHLIGHT_L10N_IDs = new Map([
  ["price", "shopping-highlight-price"],
  ["quality", "shopping-highlight-quality"],
  ["shipping", "shopping-highlight-shipping"],
  ["competitiveness", "shopping-highlight-competitiveness"],
  ["packaging/appearance", "shopping-highlight-packaging"],
]);

/**
 * Class for displaying all available highlight categories for a product and any
 * highlight reviews per category.
 */
class ReviewHighlights extends MozLitElement {
  /**
   * highlightsMap is a map of highlight categories to an array of reviews per category.
   * It is possible for a category to have no reviews.
   */
  #highlightsMap;

  static properties = {
    highlights: { type: Object },
  };

  static get queries() {
    return {
      reviewHighlightsListEl: "#review-highlights-list",
    };
  }

  updateHighlightsMap() {
    let availableKeys;
    this.#highlightsMap = null;

    try {
      if (!this.highlights) {
        return;
      }

      // Filter highlights that have data.
      let keys = Object.keys(this.highlights);
      availableKeys = keys.filter(
        key => Object.values(this.highlights[key]).flat().length !== 0
      );

      // Filter valid highlight category types. Valid types are guaranteed to have data-l10n-ids.
      availableKeys = availableKeys.filter(key =>
        VALID_HIGHLIGHT_L10N_IDs.has(key)
      );

      // If there are no highlights to show in the end, stop here and don't render content.
      if (!availableKeys.length) {
        return;
      }
    } catch (e) {
      return;
    }

    this.#highlightsMap = new Map();

    for (let key of availableKeys) {
      // Ignore negative,neutral,positive sentiments and simply append review strings into one array.
      let reviews = Object.values(this.highlights[key]).flat();
      this.#highlightsMap.set(key, reviews);
    }
  }

  createHighlightElement(type, reviews) {
    let highlightEl = document.createElement("highlight-item");
    // We already verify highlight type and its l10n id in updateHighlightsMap.
    let l10nId = VALID_HIGHLIGHT_L10N_IDs.get(type);
    highlightEl.id = type;
    highlightEl.l10nId = l10nId;
    highlightEl.highlightType = type;
    highlightEl.reviews = reviews;
    // At present, en is only supported. Once we support more locales,
    // update this attribute accordingly.
    highlightEl.lang = "en";
    return highlightEl;
  }

  render() {
    this.updateHighlightsMap();

    if (!this.#highlightsMap) {
      this.hidden = true;
      return null;
    }

    this.hidden = false;

    let highlightsTemplate = [];
    for (let [key, value] of this.#highlightsMap) {
      let highlightEl = this.createHighlightElement(key, value);
      highlightsTemplate.push(highlightEl);
    }

    // Only use show-more card type if there are more than two highlights.
    let isShowMore = Array.from(this.#highlightsMap.values()).flat().length > 2;

    return html`
      <shopping-card
        data-l10n-id="shopping-highlights-label"
        data-l10n-attrs="label"
        type=${isShowMore ? "show-more" : ""}
      >
        <div slot="content" id="review-highlights-wrapper">
          <dl id="review-highlights-list">${highlightsTemplate}</dl>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("review-highlights", ReviewHighlights);
