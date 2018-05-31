/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import PaymentStateSubscriberMixin from "../mixins/PaymentStateSubscriberMixin.js";
import RichSelect from "../components/rich-select.js";
import ShippingOption from "../components/shipping-option.js";

/**
 * <shipping-option-picker></shipping-option-picker>
 * Container around <rich-select> with
 * <option> listening to shippingOptions.
 */

export default class ShippingOptionPicker extends PaymentStateSubscriberMixin(HTMLElement) {
  constructor() {
    super();
    this.dropdown = new RichSelect();
    this.dropdown.addEventListener("change", this);
    this.dropdown.setAttribute("option-type", "shipping-option");
  }

  connectedCallback() {
    this.appendChild(this.dropdown);
    super.connectedCallback();
  }

  render(state) {
    let {shippingOptions} = state.request.paymentDetails;
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

    if (selectedShippingOption && selectedShippingOption !== this.dropdown.popupBox.value) {
      throw new Error(`The option ${selectedShippingOption} ` +
                      `does not exist in the shipping option picker`);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "change": {
        this.onChange(event);
        break;
      }
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
