/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

const MIN_SHOW_MORE_HEIGHT = 200;
/**
 * A card container to be used in the shopping sidebar. There are three card types.
 * The default type where no type attribute is required and the card will have no extra functionality.
 * The "accordion" type will initially not show any content. The card will contain a arrow to expand the
 * card so all of the content is visible.
 *
 * @property {string} label - The label text that will be used for the card header
 * @property {string} type - (optional) The type of card. No type specified
 *   will be the default card. The other available types are "accordion" and "show-more".
 */
class ShoppingCard extends MozLitElement {
  static properties = {
    label: { type: String },
    type: { type: String },
    _isExpanded: { type: Boolean },
  };

  static get queries() {
    return {
      detailsEl: "#shopping-details",
      contentEl: "#content",
    };
  }

  labelTemplate() {
    if (this.label) {
      if (this.type === "accordion") {
        return html`
          <div id="label-wrapper">
            <h2 id="header">${this.label}</h2>
            <button
              tabindex="-1"
              class="icon chevron-icon ghost-button"
              aria-labelledby="header"
              @click=${this.handleChevronButtonClick}
            ></button>
          </div>
        `;
      }
      return html`
        <div id="label-wrapper">
          <h2 id="header">${this.label}</h2>
          <slot name="rating"></slot>
        </div>
      `;
    }
    return "";
  }

  cardTemplate() {
    if (this.type === "accordion") {
      return html`
        <details id="shopping-details" @toggle=${this.onCardToggle}>
          <summary>${this.labelTemplate()}</summary>
          <div id="content"><slot name="content"></slot></div>
        </details>
      `;
    } else if (this.type === "show-more") {
      return html`
        ${this.labelTemplate()}
        <article
          id="content"
          class="show-more"
          aria-describedby="content"
          expanded="false"
        >
          <slot name="content"></slot>

          <footer>
            <button
              aria-controls="content"
              class="small-button shopping-button"
              data-l10n-id="shopping-show-more-button"
              @click=${this.handleShowMoreButtonClick}
            ></button>
          </footer>
        </article>
      `;
    }
    return html`
      ${this.labelTemplate()}
      <div id="content" aria-describedby="content">
        <slot name="content"></slot>
      </div>
    `;
  }

  onCardToggle() {
    const action = this.detailsEl.open ? "expanded" : "collapsed";
    let l10nId = this.getAttribute("data-l10n-id");
    switch (l10nId) {
      case "shopping-settings-label":
        Glean.shopping.surfaceSettingsExpandClicked.record({ action });
        break;
      case "shopping-analysis-explainer-label":
        Glean.shopping.surfaceShowQualityExplainerClicked.record({
          action,
        });
        break;
    }
  }

  handleShowMoreButtonClick(e) {
    this._isExpanded = !this._isExpanded;
    // toggle show more/show less text
    e.target.setAttribute(
      "data-l10n-id",
      this._isExpanded
        ? "shopping-show-less-button"
        : "shopping-show-more-button"
    );
    // toggle content expanded attribute
    this.contentEl.attributes.expanded.value = this._isExpanded;

    let action = this._isExpanded ? "expanded" : "collapsed";
    Glean.shopping.surfaceShowMoreReviewsButtonClicked.record({
      action,
    });
  }

  enableShowMoreButton() {
    this._isExpanded = false;
    this.toggleAttribute("showMoreButtonDisabled", false);
    this.contentEl.attributes.expanded.value = false;
  }

  disableShowMoreButton() {
    this._isExpanded = true;
    this.toggleAttribute("showMoreButtonDisabled", true);
    this.contentEl.attributes.expanded.value = true;
  }

  handleChevronButtonClick() {
    this.detailsEl.open = !this.detailsEl.open;
  }

  firstUpdated() {
    if (this.type !== "show-more") {
      return;
    }

    let contentSlot = this.shadowRoot.querySelector("slot[name='content']");
    let contentSlotEls = contentSlot.assignedElements();
    if (!contentSlotEls.length) {
      return;
    }

    let slottedDiv = contentSlotEls[0];

    this.handleContentSlotResize = this.handleContentSlotResize.bind(this);
    this.contentResizeObserver = new ResizeObserver(
      this.handleContentSlotResize
    );
    this.contentResizeObserver.observe(slottedDiv);
  }

  disconnectedCallback() {
    this.contentResizeObserver.disconnect();
  }

  handleContentSlotResize(entries) {
    for (let entry of entries) {
      if (entry.contentRect.height === 0) {
        return;
      }

      if (entry.contentRect.height < MIN_SHOW_MORE_HEIGHT) {
        this.disableShowMoreButton();
      } else if (this.hasAttribute("showMoreButtonDisabled")) {
        this.enableShowMoreButton();
      }
    }
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-card.css"
      />
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-page.css"
      />
      <article
        class="shopping-card"
        aria-labelledby="header"
        aria-label=${ifDefined(this.label)}
      >
        ${this.cardTemplate()}
      </article>
    `;
  }
}
customElements.define("shopping-card", ShoppingCard);
