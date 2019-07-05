/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import AddressForm from "./address-form.js";
import AddressOption from "../components/address-option.js";
import RichPicker from "./rich-picker.js";
import paymentRequest from "../paymentRequest.js";
import HandleEventMixin from "../mixins/HandleEventMixin.js";

/**
 * <address-picker></address-picker>
 * Container around add/edit links and <rich-select> with
 * <address-option> listening to savedAddresses & tempAddresses.
 */

export default class AddressPicker extends HandleEventMixin(RichPicker) {
  static get pickerAttributes() {
    return ["address-fields", "break-after-nth-field", "data-field-separator"];
  }

  static get observedAttributes() {
    return RichPicker.observedAttributes.concat(AddressPicker.pickerAttributes);
  }

  constructor() {
    super();
    this.dropdown.setAttribute("option-type", "address-option");
  }

  attributeChangedCallback(name, oldValue, newValue) {
    super.attributeChangedCallback(name, oldValue, newValue);
    // connectedCallback may add and adjust elements & values
    // so avoid calling render before the element is connected
    if (
      this.isConnected &&
      AddressPicker.pickerAttributes.includes(name) &&
      oldValue !== newValue
    ) {
      this.render(this.requestStore.getState());
    }
  }

  get fieldNames() {
    if (this.hasAttribute("address-fields")) {
      let names = this.getAttribute("address-fields")
        .trim()
        .split(/\s+/);
      if (names.length) {
        return names;
      }
    }

    return [
      // "address-level1", // TODO: bug 1481481 - not required for some countries e.g. DE
      "address-level2",
      "country",
      "name",
      "postal-code",
      "street-address",
    ];
  }

  /**
   * De-dupe and filter addresses for the given set of fields that will be visible
   *
   * @param {object} addresses
   * @param {array?} fieldNames - optional list of field names that be used when
   *                              de-duping and excluding entries
   * @returns {object} filtered copy of given addresses
   */
  filterAddresses(addresses, fieldNames = this.fieldNames) {
    let uniques = new Set();
    let result = {};
    for (let [guid, address] of Object.entries(addresses)) {
      let addressCopy = {};
      let isMatch = false;
      // exclude addresses that are missing all of the requested fields
      for (let name of fieldNames) {
        if (address[name]) {
          isMatch = true;
          addressCopy[name] = address[name];
        }
      }
      if (isMatch) {
        let key = JSON.stringify(addressCopy);
        // exclude duplicated addresses
        if (!uniques.has(key)) {
          uniques.add(key);
          result[guid] = address;
        }
      }
    }
    return result;
  }

  get options() {
    return this.dropdown.popupBox.options;
  }

  /**
   * @param {object} state - See `PaymentsStore.setState`
   * The value of the picker is retrieved from state store rather than the DOM
   * @returns {string} guid
   */
  getCurrentValue(state) {
    let [selectedKey, selectedLeaf] = this.selectedStateKey.split("|");
    let guid = state[selectedKey];
    if (selectedLeaf) {
      guid = guid[selectedLeaf];
    }
    return guid;
  }

  render(state) {
    let selectedAddressGUID = this.getCurrentValue(state) || "";
    let addresses = paymentRequest.getAddresses(state);
    let desiredOptions = [];
    let filteredAddresses = this.filterAddresses(addresses, this.fieldNames);
    for (let [guid, address] of Object.entries(filteredAddresses)) {
      let optionEl = this.dropdown.getOptionByValue(guid);
      if (!optionEl) {
        optionEl = document.createElement("option");
        optionEl.value = guid;
      }

      for (let key of AddressOption.recordAttributes) {
        let val = address[key];
        if (val) {
          optionEl.setAttribute(key, val);
        } else {
          optionEl.removeAttribute(key);
        }
      }

      optionEl.dataset.fieldSeparator = this.dataset.fieldSeparator;

      if (this.hasAttribute("address-fields")) {
        optionEl.setAttribute(
          "address-fields",
          this.getAttribute("address-fields")
        );
      } else {
        optionEl.removeAttribute("address-fields");
      }

      if (this.hasAttribute("break-after-nth-field")) {
        optionEl.setAttribute(
          "break-after-nth-field",
          this.getAttribute("break-after-nth-field")
        );
      } else {
        optionEl.removeAttribute("break-after-nth-field");
      }

      // fieldNames getter is not used here because it returns a default array with
      // attributes even when "address-fields" observed attribute is null.
      let addressFields = this.getAttribute("address-fields");
      optionEl.textContent = AddressOption.formatSingleLineLabel(
        address,
        addressFields
      );
      desiredOptions.push(optionEl);
    }

    this.dropdown.popupBox.textContent = "";

    if (this._allowEmptyOption) {
      let optionEl = document.createElement("option");
      optionEl.value = "";
      desiredOptions.unshift(optionEl);
    }

    for (let option of desiredOptions) {
      this.dropdown.popupBox.appendChild(option);
    }

    // Update selectedness after the options are updated
    this.dropdown.value = selectedAddressGUID;

    if (selectedAddressGUID && selectedAddressGUID !== this.dropdown.value) {
      throw new Error(
        `${this.selectedStateKey} option ${selectedAddressGUID} ` +
          `does not exist in the address picker`
      );
    }

    super.render(state);
  }

  get selectedStateKey() {
    return this.getAttribute("selected-state-key");
  }

  errorForSelectedOption(state) {
    let superError = super.errorForSelectedOption(state);
    if (superError) {
      return superError;
    }

    if (!this.selectedOption) {
      return "";
    }

    let merchantFieldErrors = AddressForm.merchantFieldErrorsForForm(
      state,
      this.selectedStateKey.split("|")
    );
    // TODO: errors in priority order.
    return (
      Object.values(merchantFieldErrors).find(msg => {
        return typeof msg == "string" && msg.length;
      }) || ""
    );
  }

  onChange(event) {
    let [selectedKey, selectedLeaf] = this.selectedStateKey.split("|");
    if (!selectedKey) {
      return;
    }
    // selectedStateKey can be a '|' delimited string indicating a path into the state object
    // to update with the new value
    let newState = {};

    if (selectedLeaf) {
      let currentState = this.requestStore.getState();
      newState[selectedKey] = Object.assign({}, currentState[selectedKey], {
        [selectedLeaf]: this.dropdown.value,
      });
    } else {
      newState[selectedKey] = this.dropdown.value;
    }
    this.requestStore.setState(newState);
  }

  onClick({ target }) {
    let pageId;
    let currentState = this.requestStore.getState();
    let nextState = {
      page: {},
    };

    switch (this.selectedStateKey) {
      case "selectedShippingAddress":
        pageId = "shipping-address-page";
        break;
      case "selectedPayerAddress":
        pageId = "payer-address-page";
        break;
      case "basic-card-page|billingAddressGUID":
        pageId = "billing-address-page";
        break;
      default: {
        throw new Error(
          "onClick, un-matched selectedStateKey: " + this.selectedStateKey
        );
      }
    }
    nextState.page.id = pageId;
    let addressFields = this.getAttribute("address-fields");
    nextState[pageId] = { addressFields };

    switch (target) {
      case this.addLink: {
        nextState[pageId].guid = null;
        break;
      }
      case this.editLink: {
        nextState[pageId].guid = this.getCurrentValue(currentState);
        break;
      }
      default: {
        throw new Error("Unexpected onClick");
      }
    }

    this.requestStore.setState(nextState);
  }
}

customElements.define("address-picker", AddressPicker);
