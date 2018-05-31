/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import ObservedPropertiesMixin from "../mixins/ObservedPropertiesMixin.js";
import RichOption from "./rich-option.js";
/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <rich-select>
 *  <address-option guid="98hgvnbmytfc"
 *                  address-level1="MI"
 *                  address-level2="Some City"
 *                  email="foo@example.com"
 *                  country="USA"
 *                  name="Jared Wein"
 *                  postal-code="90210"
 *                  street-address="1234 Anywhere St"
 *                  tel="+1 650 555-5555"></address-option>
 * </rich-select>
 *
 * Attribute names follow FormAutofillStorage.jsm.
 */

export default class AddressOption extends ObservedPropertiesMixin(RichOption) {
  static get recordAttributes() {
    return [
      "address-level1",
      "address-level2",
      "country",
      "email",
      "guid",
      "name",
      "postal-code",
      "street-address",
      "tel",
    ];
  }

  static get observedAttributes() {
    return RichOption.observedAttributes.concat(AddressOption.recordAttributes);
  }

  constructor() {
    super();

    for (let name of ["name", "street-address", "email", "tel"]) {
      this[`_${name}`] = document.createElement("span");
      this[`_${name}`].classList.add(name);
    }
  }

  connectedCallback() {
    for (let name of ["name", "street-address", "email", "tel"]) {
      this.appendChild(this[`_${name}`]);
    }
    super.connectedCallback();
  }

  static formatSingleLineLabel(address) {
    return PaymentDialogUtils.getAddressLabel(address);
  }

  render() {
    this._name.textContent = this.name;
    this["_street-address"].textContent = `${this.streetAddress} ` +
      `${this.addressLevel2} ${this.addressLevel1} ${this.postalCode} ${this.country}`;
    this._email.textContent = this.email;
    this._tel.textContent = this.tel;
  }
}

customElements.define("address-option", AddressOption);
