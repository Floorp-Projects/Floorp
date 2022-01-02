/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import RichPicker from "./rich-picker.js";
import ShippingOption from "../components/shipping-option.js";
import HandleEventMixin from "../mixins/HandleEventMixin.js";

/**
 * <shipping-option-picker></shipping-option-picker>
 * Container around <rich-select> with
 * <option> listening to shippingOptions.
 */

export default class ShippingOptionPicker extends HandleEventMixin(RichPicker) {
  constructor() {
    super();
    this.dropdown.setAttribute("option-type", "shipping-option");
  }

  render(state) {
    this.addLink.hidden = true;
    this.editLink.hidden = true;

    // If requestShipping is true but paymentDetails.shippingOptions isn't defined
    // then use an empty array as a fallback.
    let shippingOptions = state.request.paymentDetails.shippingOptions || [];
    let desiredOptions = [];
    for (let option of shippingOptions) {
      let optionEl = this.dropdown.getOptionByValue(option.id);
      if (!optionEl) {
        optionEl = document.createElement("option");
        optionEl.value = option.id;
      }

      optionEl.setAttribute("label", option.label);
      optionEl.setAttribute("amount-currency", option.amount.currency);
      optionEl.setAttribute("amount-value", option.amount.value);

      optionEl.textContent = ShippingOption.formatSingleLineLabel(option);
      desiredOptions.push(optionEl);
    }

    this.dropdown.popupBox.textContent = "";
    for (let option of desiredOptions) {
      this.dropdown.popupBox.appendChild(option);
    }

    // Update selectedness after the options are updated
    let selectedShippingOption = state.selectedShippingOption;
    this.dropdown.value = selectedShippingOption;

    if (
      selectedShippingOption &&
      selectedShippingOption !== this.dropdown.popupBox.value
    ) {
      throw new Error(
        `The option ${selectedShippingOption} ` +
          `does not exist in the shipping option picker`
      );
    }
  }

  onChange(event) {
    let selectedOptionId = this.dropdown.value;
    this.requestStore.setState({
      selectedShippingOption: selectedOptionId,
    });
  }
}

customElements.define("shipping-option-picker", ShippingOptionPicker);
