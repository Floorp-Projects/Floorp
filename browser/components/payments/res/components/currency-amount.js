/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * <currency-amount value="7.5" currency="USD" display-code></currency-amount>
 */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";

export default class CurrencyAmount extends ObservedPropertiesMixin(HTMLElement) {
  static get observedAttributes() {
    return [
      "currency",
      "display-code",
      "value",
    ];
  }

  constructor() {
    super();
    this._currencyAmountTextNode = document.createTextNode("");
    this._currencyCodeElement = document.createElement("span");
    this._currencyCodeElement.classList.add("currency-code");
  }

  connectedCallback() {
    if (super.connectedCallback) {
      super.connectedCallback();
    }

    this.append(this._currencyAmountTextNode, this._currencyCodeElement);
  }

  render() {
    let currencyAmount = "";
    let currencyCode = "";
    try {
      if (this.value && this.currency) {
        let number = Number.parseFloat(this.value);
        if (Number.isNaN(number) || !Number.isFinite(number)) {
          throw new RangeError("currency-amount value must be a finite number");
        }
        const symbolFormatter = new Intl.NumberFormat(navigator.languages, {
          style: "currency",
          currency: this.currency,
          currencyDisplay: "symbol",
        });
        currencyAmount = symbolFormatter.format(this.value);

        if (this.displayCode !== null) {
          // XXX: Bug 1473772 will move the separator to a Fluent string.
          currencyAmount += " ";

          const codeFormatter = new Intl.NumberFormat(navigator.languages, {
            style: "currency",
            currency: this.currency,
            currencyDisplay: "code",
          });
          let parts = codeFormatter.formatToParts(this.value);
          let currencyPart = parts.find(part => part.type == "currency");
          currencyCode = currencyPart.value;
        }
      }
    } finally {
      this._currencyAmountTextNode.textContent = currencyAmount;
      this._currencyCodeElement.textContent = currencyCode;
    }
  }
}

customElements.define("currency-amount", CurrencyAmount);
