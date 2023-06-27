/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";

import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

// eslint-disable-next-line import/no-unassigned-import
import "chrome://browser/content/shopping/highlight-item.mjs";

const { ShoppingUtils } = ChromeUtils.importESModule(
  "chrome://browser/content/shopping/ShoppingUtils.sys.mjs"
);

const VALID_HIGHLIGHT_L10N_IDs = new Map([
  ["price", "shopping-highlight-price"],
  ["quality", "shopping-highlight-quality"],
  ["shipping", "shopping-highlight-shipping"],
  ["competitiveness", "shopping-highlight-competitiveness"],
  ["packaging", "shopping-highlight-packaging"],
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

  static get queries() {
    return {
      reviewHighlightsListEl: "#review-highlights-list",
    };
  }

  connectedCallback() {
    super.connectedCallback();

    let highlights;
    let availableKeys;

    try {
      highlights = ShoppingUtils.getHighlights();
      if (!highlights) {
        return;
      } else if (highlights.error) {
        throw new Error(
          "Unable to fetch highlights due to error: " + highlights.error
        );
      }

      // Filter highlights that have data.
      let keys = Object.keys(highlights);
      availableKeys = keys.filter(
        key => Object.values(highlights[key]).flat().length !== 0
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

    this.hidden = false;
    this.#highlightsMap = new Map();

    for (let key of availableKeys) {
      // Ignore negative,neutral,positive sentiments and simply append review strings into one array.
      let reviews = Object.values(highlights[key]).flat();
      this.#highlightsMap.set(key, reviews);
    }
  }

  createHighlightElement(type, reviews) {
    let highlightEl = document.createElement("highlight-item");
    // We already verify highlight type and its l10n id in connectedCallback.
    let l10nId = VALID_HIGHLIGHT_L10N_IDs.get(type);
    highlightEl.id = type;
    highlightEl.l10nId = l10nId;
    highlightEl.reviews = reviews;
    // At present, en is only supported. Once we support more locales,
    // update this attribute accordingly.
    highlightEl.lang = "en";
    return highlightEl;
  }

  render() {
    if (!this.#highlightsMap) {
      return null;
    }

    let highlightsTemplate = [];
    for (let [key, value] of this.#highlightsMap) {
      let highlightEl = this.createHighlightElement(key, value);
      highlightsTemplate.push(highlightEl);
    }
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/highlights.css"
      />
      <div id="review-highlights-wrapper">
        <h2
          id="review-highlights-heading"
          data-l10n-id="shopping-highlights-heading"
        ></h2>
        <dl id="review-highlights-list">${highlightsTemplate}</dl>
      </div>
    `;
  }
}

customElements.define("review-highlights", ReviewHighlights);
