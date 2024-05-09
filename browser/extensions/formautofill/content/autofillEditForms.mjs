/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
});

class EditAutofillForm {
  constructor(elements) {
    this._elements = elements;
  }

  /**
   * Fill the form with a record object.
   *
   * @param  {object} [record = {}]
   */
  loadRecord(record = {}) {
    for (let field of this._elements.form.elements) {
      let value = record[field.id];
      value = typeof value == "undefined" ? "" : value;

      if (record.guid) {
        field.value = value;
      } else if (field.localName == "select") {
        this.setDefaultSelectedOptionByValue(field, value);
      } else {
        // Use .defaultValue instead of .value to avoid setting the `dirty` flag
        // which triggers form validation UI.
        field.defaultValue = value;
      }
    }
    if (!record.guid) {
      // Reset the dirty value flag and validity state.
      this._elements.form.reset();
    } else {
      for (let field of this._elements.form.elements) {
        this.updatePopulatedState(field);
        this.updateCustomValidity(field);
      }
    }
  }

  setDefaultSelectedOptionByValue(select, value) {
    for (let option of select.options) {
      option.defaultSelected = option.value == value;
    }
  }

  /**
   * Get a record from the form suitable for a save/update in storage.
   *
   * @returns {object}
   */
  buildFormObject() {
    let initialObject = {};
    if (this.hasMailingAddressFields) {
      // Start with an empty string for each mailing-address field so that any
      // fields hidden for the current country are blanked in the return value.
      initialObject = {
        "street-address": "",
        "address-level3": "",
        "address-level2": "",
        "address-level1": "",
        "postal-code": "",
      };
    }

    return Array.from(this._elements.form.elements).reduce((obj, input) => {
      if (!input.disabled) {
        obj[input.id] = input.value;
      }
      return obj;
    }, initialObject);
  }

  /**
   * Handle events
   *
   * @param  {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "change": {
        this.handleChange(event);
        break;
      }
      case "input": {
        this.handleInput(event);
        break;
      }
    }
  }

  /**
   * Handle change events
   *
   * @param  {DOMEvent} event
   */
  handleChange(event) {
    this.updatePopulatedState(event.target);
  }

  /**
   * Handle input events
   */
  handleInput(_e) {}

  /**
   * Attach event listener
   */
  attachEventListeners() {
    this._elements.form.addEventListener("input", this);
  }

  /**
   * Set the field-populated attribute if the field has a value.
   *
   * @param {DOMElement} field The field that will be checked for a value.
   */
  updatePopulatedState(field) {
    let span = field.parentNode.querySelector(".label-text");
    if (!span) {
      return;
    }
    span.toggleAttribute("field-populated", !!field.value.trim());
  }

  /**
   * Run custom validity routines specific to the field and type of form.
   *
   * @param {DOMElement} _field The field that will be validated.
   */
  updateCustomValidity(_field) {}
}

export class EditCreditCard extends EditAutofillForm {
  /**
   * @param {HTMLElement[]} elements
   * @param {object} record with a decrypted cc-number
   * @param {object} addresses in an object with guid keys for the billing address picker.
   */
  constructor(elements, record, addresses) {
    super(elements);

    this._addresses = addresses;
    Object.assign(this._elements, {
      ccNumber: this._elements.form.querySelector("#cc-number"),
      invalidCardNumberStringElement: this._elements.form.querySelector(
        "#invalidCardNumberString"
      ),
      month: this._elements.form.querySelector("#cc-exp-month"),
      year: this._elements.form.querySelector("#cc-exp-year"),
      billingAddress: this._elements.form.querySelector("#billingAddressGUID"),
      billingAddressRow:
        this._elements.form.querySelector(".billingAddressRow"),
    });

    this.attachEventListeners();
    this.loadRecord(record, addresses);
  }

  loadRecord(record, addresses, preserveFieldValues) {
    // _record must be updated before generateYears and generateBillingAddressOptions are called.
    this._record = record;
    this._addresses = addresses;
    this.generateBillingAddressOptions(preserveFieldValues);
    if (!preserveFieldValues) {
      // Re-generating the months will reset the selected option.
      this.generateMonths();
      // Re-generating the years will reset the selected option.
      this.generateYears();
      super.loadRecord(record);
    }
  }

  generateMonths() {
    const count = 12;

    // Clear the list
    this._elements.month.textContent = "";

    // Empty month option
    this._elements.month.appendChild(new Option());

    // Populate month list. Format: "month number - month name"
    let dateFormat = new Intl.DateTimeFormat(navigator.language, {
      month: "long",
    }).format;
    for (let i = 0; i < count; i++) {
      let monthNumber = (i + 1).toString();
      let monthName = dateFormat(new Date(1970, i));
      let option = new Option();
      option.value = monthNumber;
      // XXX: Bug 1446164 - Localize this string.
      option.textContent = `${monthNumber.padStart(2, "0")} - ${monthName}`;
      this._elements.month.appendChild(option);
    }
  }

  generateYears() {
    const count = 11;
    const currentYear = new Date().getFullYear();
    const ccExpYear = this._record && this._record["cc-exp-year"];

    // Clear the list
    this._elements.year.textContent = "";

    // Provide an empty year option
    this._elements.year.appendChild(new Option());

    if (ccExpYear && ccExpYear < currentYear) {
      this._elements.year.appendChild(new Option(ccExpYear));
    }

    for (let i = 0; i < count; i++) {
      let year = currentYear + i;
      let option = new Option(year);
      this._elements.year.appendChild(option);
    }

    if (ccExpYear && ccExpYear > currentYear + count) {
      this._elements.year.appendChild(new Option(ccExpYear));
    }
  }

  generateBillingAddressOptions(preserveFieldValues) {
    let billingAddressGUID;
    if (preserveFieldValues && this._elements.billingAddress.value) {
      billingAddressGUID = this._elements.billingAddress.value;
    } else if (this._record) {
      billingAddressGUID = this._record.billingAddressGUID;
    }

    this._elements.billingAddress.textContent = "";

    this._elements.billingAddress.appendChild(new Option("", ""));

    let hasAddresses = false;
    for (let [guid, address] of Object.entries(this._addresses)) {
      hasAddresses = true;
      let selected = guid == billingAddressGUID;
      let option = new Option(
        lazy.FormAutofillUtils.getAddressLabel(address),
        guid,
        selected,
        selected
      );
      this._elements.billingAddress.appendChild(option);
    }

    this._elements.billingAddressRow.hidden = !hasAddresses;
  }

  attachEventListeners() {
    this._elements.form.addEventListener("change", this);
    super.attachEventListeners();
  }

  handleInput(event) {
    // Clear the error message if cc-number is valid
    if (
      event.target == this._elements.ccNumber &&
      lazy.FormAutofillUtils.isCCNumber(this._elements.ccNumber.value)
    ) {
      this._elements.ccNumber.setCustomValidity("");
    }
    super.handleInput(event);
  }

  updateCustomValidity(field) {
    super.updateCustomValidity(field);

    // Mark the cc-number field as invalid if the number is empty or invalid.
    if (
      field == this._elements.ccNumber &&
      !lazy.FormAutofillUtils.isCCNumber(field.value)
    ) {
      let invalidCardNumberString =
        this._elements.invalidCardNumberStringElement.textContent;
      field.setCustomValidity(invalidCardNumberString || " ");
    }
  }
}
