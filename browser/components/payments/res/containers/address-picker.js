/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import AddressOption from "../components/address-option.js";
import RichPicker from "./rich-picker.js";
import paymentRequest from "../paymentRequest.js";

/**
 * <address-picker></address-picker>
 * Container around add/edit links and <rich-select> with
 * <address-option> listening to savedAddresses & tempAddresses.
 */

export default class AddressPicker extends RichPicker {
  static get observedAttributes() {
    return RichPicker.observedAttributes.concat(["address-fields"]);
  }

  constructor() {
    super();
    this.dropdown.setAttribute("option-type", "address-option");
  }

  attributeChangedCallback(name, oldValue, newValue) {
    super.attributeChangedCallback(name, oldValue, newValue);
    if (name == "address-fields" && oldValue !== newValue) {
      this.render(this.requestStore.getState());
    }
  }

  /**
   * De-dupe and filter addresses for the given set of fields that will be visible
   *
   * @param {object} addresses
   * @param {array?} fieldNames - optional list of field names that be used when
   *                              de-duping and excluding entries
   * @returns {object} filtered copy of given addresses
   */
  filterAddresses(addresses, fieldNames = [
    "address-level1",
    "address-level2",
    "country",
    "name",
    "postal-code",
    "street-address",
  ]) {
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

  render(state) {
    let addresses = paymentRequest.getAddresses(state);
    let desiredOptions = [];
    let fieldNames;
    if (this.hasAttribute("address-fields")) {
      let names = this.getAttribute("address-fields").split(/\s+/);
      if (names.length) {
        fieldNames = names;
      }
    }
    let filteredAddresses = this.filterAddresses(addresses, fieldNames);
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

      optionEl.textContent = AddressOption.formatSingleLineLabel(address);
      desiredOptions.push(optionEl);
    }

    this.dropdown.popupBox.textContent = "";
    for (let option of desiredOptions) {
      this.dropdown.popupBox.appendChild(option);
    }

    // Update selectedness after the options are updated
    let selectedAddressGUID = state[this.selectedStateKey];
    this.dropdown.value = selectedAddressGUID;

    if (selectedAddressGUID && selectedAddressGUID !== this.dropdown.value) {
      throw new Error(`${this.selectedStateKey} option ${selectedAddressGUID}` +
                      `does not exist in the address picker`);
    }
  }

  get selectedStateKey() {
    return this.getAttribute("selected-state-key");
  }

  handleEvent(event) {
    switch (event.type) {
      case "change": {
        this.onChange(event);
        break;
      }
      case "click": {
        this.onClick(event);
      }
    }
  }

  onChange(event) {
    let selectedKey = this.selectedStateKey;
    if (selectedKey) {
      this.requestStore.setState({
        [selectedKey]: this.dropdown.value,
      });
    }
  }

  onClick({target}) {
    let nextState = {
      page: {
        id: "address-page",
        selectedStateKey: [this.selectedStateKey],
      },
      "address-page": {
        addressFields: this.getAttribute("address-fields"),
      },
    };

    switch (target) {
      case this.addLink: {
        nextState["address-page"].guid = null;
        nextState["address-page"].title = this.dataset.addAddressTitle;
        break;
      }
      case this.editLink: {
        let state = this.requestStore.getState();
        let selectedAddressGUID = state[this.selectedStateKey];
        nextState["address-page"].guid = selectedAddressGUID;
        nextState["address-page"].title = this.dataset.editAddressTitle;
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
