/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported EditAddress, EditCreditCard */
/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

"use strict";

class EditAutofillForm {
  constructor(elements) {
    this._elements = elements;
  }

  /**
   * Fill the form with a record object.
   * @param  {object} [record = {}]
   */
  loadRecord(record = {}) {
    for (let field of this._elements.form.elements) {
      let value = record[field.id];
      field.value = typeof(value) == "undefined" ? "" : value;
    }
  }

  /**
   * Get inputs from the form.
   * @returns {object}
   */
  buildFormObject() {
    return Array.from(this._elements.form.elements).reduce((obj, input) => {
      if (input.value) {
        obj[input.id] = input.value;
      }
      return obj;
    }, {});
  }

  /**
   * Handle events
   *
   * @param  {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "input": {
        this.handleInput(event);
        break;
      }
      case "change": {
        this.handleChange(event);
        break;
      }
    }
  }

  /**
   * Handle input events
   *
   * @param  {DOMEvent} event
   */
  handleInput(event) {}

  /**
   * Attach event listener
   */
  attachEventListeners() {
    this._elements.form.addEventListener("input", this);
  }

  // An interface to be inherited.
  handleChange(event) {}
}

class EditAddress extends EditAutofillForm {
  /**
   * @param {HTMLElement[]} elements
   * @param {object} record
   * @param {object} config
   * @param {string[]} config.DEFAULT_REGION
   * @param {function} config.getFormFormat Function to return form layout info for a given country.
   * @param {string[]} config.supportedCountries
   */
  constructor(elements, record, config) {
    super(elements);

    Object.assign(this, config);
    Object.assign(this._elements, {
      addressLevel1Label: this._elements.form.querySelector("#address-level1-container > span"),
      postalCodeLabel: this._elements.form.querySelector("#postal-code-container > span"),
      country: this._elements.form.querySelector("#country"),
    });

    this.populateCountries();
    // Need to populate the countries before trying to set the initial country.
    // Also need to use this._record so it has the default country selected.
    this.loadRecord(record);
    this.attachEventListeners();
  }

  loadRecord(record) {
    this._record = record;
    if (!record) {
      record = {
        country: this.supportedCountries.find(supported => supported == this.DEFAULT_REGION),
      };
    }
    super.loadRecord(record);
    this.formatForm(record.country);
  }

  /**
   * Format the form based on country. The address-level1 and postal-code labels
   * should be specific to the given country.
   * @param  {string} country
   */
  formatForm(country) {
    const {addressLevel1Label, postalCodeLabel, fieldsOrder} = this.getFormFormat(country);
    this._elements.addressLevel1Label.dataset.localization = addressLevel1Label;
    this._elements.postalCodeLabel.dataset.localization = postalCodeLabel;
    this.arrangeFields(fieldsOrder);
  }

  arrangeFields(fieldsOrder) {
    let fields = [
      "name",
      "organization",
      "street-address",
      "address-level2",
      "address-level1",
      "postal-code",
    ];
    let inputs = [];
    for (let i = 0; i < fieldsOrder.length; i++) {
      let {fieldId, newLine} = fieldsOrder[i];
      let container = document.getElementById(`${fieldId}-container`);
      inputs.push(...container.querySelectorAll("input, textarea, select"));
      container.style.display = "flex";
      container.style.order = i;
      container.style.pageBreakAfter = newLine ? "always" : "auto";
      // Remove the field from the list of fields
      fields.splice(fields.indexOf(fieldId), 1);
    }
    for (let i = 0; i < inputs.length; i++) {
      // Assign tabIndex starting from 1
      inputs[i].tabIndex = i + 1;
    }
    // Hide the remaining fields
    for (let field of fields) {
      let container = document.getElementById(`${field}-container`);
      container.style.display = "none";
    }
  }

  populateCountries() {
    let fragment = document.createDocumentFragment();
    for (let country of this.supportedCountries) {
      let option = new Option();
      option.value = country;
      option.dataset.localizationRegion = country.toLowerCase();
      fragment.appendChild(option);
    }
    this._elements.country.appendChild(fragment);
  }

  handleChange(event) {
    this.formatForm(event.target.value);
  }

  attachEventListeners() {
    this._elements.country.addEventListener("change", this);
    super.attachEventListeners();
  }
}

class EditCreditCard extends EditAutofillForm {
  /**
   * @param {HTMLElement[]} elements
   * @param {object} record with a decrypted cc-number
   * @param {object} config
   * @param {function} config.isCCNumber Function to determine is a string is a valid CC number.
   */
  constructor(elements, record, config) {
    super(elements);

    Object.assign(this, config);
    Object.assign(this._elements, {
      ccNumber: this._elements.form.querySelector("#cc-number"),
      year: this._elements.form.querySelector("#cc-exp-year"),
    });

    this.loadRecord(record);
    this.attachEventListeners();
  }

  loadRecord(record) {
    // _record must be updated before generateYears is called.
    this._record = record;
    this.generateYears();
    super.loadRecord(record);
  }

  generateYears() {
    const count = 11;
    const currentYear = new Date().getFullYear();
    const ccExpYear = this._record && this._record["cc-exp-year"];

    // Clear the list
    this._elements.year.textContent = "";

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

  attachEventListeners() {
    this._elements.ccNumber.addEventListener("change", this);
    super.attachEventListeners();
  }

  handleChange(event) {
    super.handleChange(event);

    if (event.target != this._elements.ccNumber) {
      return;
    }

    let ccNumberField = this._elements.ccNumber;

    // Mark the cc-number field as invalid if the number is empty or invalid.
    if (!this.isCCNumber(ccNumberField.value)) {
      ccNumberField.setCustomValidity(true);
    }
  }

  handleInput(event) {
    // Clear the error message if cc-number is valid
    if (event.target == this._elements.ccNumber &&
        this.isCCNumber(this._elements.ccNumber.value)) {
      this._elements.ccNumber.setCustomValidity("");
    }
    super.handleInput(event);
  }
}
