/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import AddressPicker from "./address-picker.js";
/* import-globals-from ../unprivileged-fallbacks.js */

/**
 * <billing-address-picker></billing-address-picker>
 * Extends AddressPicker to treat the <select>'s value as the source of truth
 */

export default class BillingAddressPicker extends AddressPicker {
  constructor() {
    super();
    this._allowEmptyOption = true;
  }

  /**
   * @param {object?} state - See `PaymentsStore.setState`
   * The value of the picker is the child dropdown element's value
   * @returns {string} guid
   */
  getCurrentValue() {
    return this.dropdown.value;
  }

  onChange(event) {
    this.render(this.requestStore.getState());
  }
}

customElements.define("billing-address-picker", BillingAddressPicker);
