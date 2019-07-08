/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";

/**
 *  <csc-input placeholder="CVV*"
               default-value="123"
               front-tooltip="Look on front of card for CSC"
               back-tooltip="Look on back of card for CSC"></csc-input>
 */

export default class CscInput extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "back-tooltip",
      "card-type",
      "default-value",
      "disabled",
      "front-tooltip",
      "placeholder",
      "value",
    ];
  }
  constructor({ useAlwaysVisiblePlaceholder, inputId } = {}) {
    super();

    this.useAlwaysVisiblePlaceholder = useAlwaysVisiblePlaceholder;

    this._input = document.createElement("input");
    this._input.id = inputId || "";
    this._input.setAttribute("type", "text");
    this._input.autocomplete = "off";
    this._input.size = 3;
    this._input.required = true;
    // 3 or more digits
    this._input.pattern = "[0-9]{3,}";
    this._input.classList.add("security-code");
    if (useAlwaysVisiblePlaceholder) {
      this._label = document.createElement("span");
      this._label.dataset.localization = "cardCVV";
      this._label.className = "label-text";
    }
    this._tooltip = document.createElement("span");
    this._tooltip.className = "info-tooltip csc";
    this._tooltip.setAttribute("tabindex", "0");
    this._tooltip.setAttribute("role", "tooltip");

    // The parent connectedCallback calls its render method before
    // our connectedCallback can run. This causes issues for parent
    // code that is looking for all the form elements. Thus, we
    // append the children during the constructor to make sure they
    // be part of the DOM sooner.
    this.appendChild(this._input);
    if (this.useAlwaysVisiblePlaceholder) {
      this.appendChild(this._label);
    }
    this.appendChild(this._tooltip);
  }

  connectedCallback() {
    this.render();
  }

  render() {
    if (this.defaultValue) {
      let oldDefaultValue = this._input.defaultValue;
      this._input.defaultValue = this.defaultValue;
      if (this._input.defaultValue != oldDefaultValue) {
        // Setting defaultValue will place a value in the field
        // but doesn't trigger a 'change' event, which is needed
        // to update the Pay button state on the summary page.
        this._input.dispatchEvent(new Event("change", { bubbles: true }));
      }
    } else {
      this._input.defaultValue = "";
    }
    if (this.value) {
      // Setting the value will trigger form validation
      // so only set the value if one has been provided.
      this._input.value = this.value;
    }
    if (this.useAlwaysVisiblePlaceholder) {
      this._label.textContent = this.placeholder || "";
    } else {
      this._input.placeholder = this.placeholder || "";
    }
    if (this.cardType == "amex") {
      this._tooltip.setAttribute("aria-label", this.frontTooltip || "");
    } else {
      this._tooltip.setAttribute("aria-label", this.backTooltip || "");
    }
  }

  get value() {
    return this._input.value;
  }

  get isValid() {
    return this._input.validity.valid;
  }

  set disabled(value) {
    // This is kept out of render() since callers
    // are expecting it to apply immediately.
    this._input.disabled = value;
    return !!value;
  }
}

customElements.define("csc-input", CscInput);
