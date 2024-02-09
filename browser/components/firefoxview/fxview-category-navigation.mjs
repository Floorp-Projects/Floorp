/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export default class FxviewCategoryNavigation extends MozLitElement {
  static properties = {
    currentCategory: { type: String },
  };

  static queries = {
    categoryButtonsSlot: "slot[name=category-button]",
  };

  get categoryButtons() {
    return this.categoryButtonsSlot
      .assignedNodes()
      .filter(node => !node.hidden);
  }

  onChangeCategory(e) {
    this.currentCategory = e.target.name;
  }

  handleFocus(e) {
    if (e.key == "ArrowDown" || e.key == "ArrowRight") {
      e.preventDefault();
      this.focusNextCategory();
    } else if (e.key == "ArrowUp" || e.key == "ArrowLeft") {
      e.preventDefault();
      this.focusPreviousCategory();
    }
  }

  focusPreviousCategory() {
    let categoryButtons = this.categoryButtons;
    let currentIndex = categoryButtons.findIndex(b => b.selected);
    let prev = categoryButtons[currentIndex - 1];
    if (prev) {
      prev.activate();
      prev.focus();
    }
  }

  focusNextCategory() {
    let categoryButtons = this.categoryButtons;
    let currentIndex = categoryButtons.findIndex(b => b.selected);
    let next = categoryButtons[currentIndex + 1];
    if (next) {
      next.activate();
      next.focus();
    }
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/fxview-category-navigation.css"
      />
      <nav>
        <div class="category-nav-header">
          <slot name="category-nav-header"></slot>
        </div>
        <div
          class="category-nav-buttons"
          role="tablist"
          aria-orientation="vertical"
        >
          <slot
            name="category-button"
            @change-category=${this.onChangeCategory}
            @keydown=${this.handleFocus}
          ></slot>
        </div>
        <div class="category-nav-footer">
          <slot name="category-nav-footer"></slot>
        </div>
      </nav>
    `;
  }

  updated() {
    let categorySelected = false;
    let assignedCategories = this.categoryButtons;
    for (let button of assignedCategories) {
      button.selected = button.name == this.currentCategory;
      categorySelected = categorySelected || button.selected;
    }
    if (!categorySelected && assignedCategories.length) {
      // Current category has no matching category, reset to the first category.
      assignedCategories[0].activate();
    }
  }
}
customElements.define("fxview-category-navigation", FxviewCategoryNavigation);

export class FxviewCategoryButton extends MozLitElement {
  static properties = {
    selected: { type: Boolean },
  };

  static queries = {
    buttonEl: "button",
  };

  connectedCallback() {
    super.connectedCallback();
    this.setAttribute("role", "tab");
  }

  get name() {
    return this.getAttribute("name");
  }

  activate() {
    this.dispatchEvent(
      new CustomEvent("change-category", {
        bubbles: true,
        composed: true,
      })
    );
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/fxview-category-button.css"
      />
      <button
        aria-hidden="true"
        tabindex="-1"
        ?selected=${this.selected}
        @click=${this.activate}
      >
        <span class="category-icon" part="icon"></span>
        <slot></slot>
      </button>
    `;
  }

  updated() {
    this.setAttribute("aria-selected", this.selected);
    this.setAttribute("tabindex", this.selected ? 0 : -1);
  }
}
customElements.define("fxview-category-button", FxviewCategoryButton);
