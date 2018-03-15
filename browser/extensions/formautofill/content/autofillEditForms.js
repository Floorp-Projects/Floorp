/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported EditAddress, EditCreditCard */
/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

"use strict";

ChromeUtils.import("resource://formautofill/FormAutofillUtils.jsm");

class EditAutofillForm {
  constructor(elements, record) {
    this._elements = elements;
    this._record = record;
  }

  /**
   * Fill the form with a record object.
   * @param  {object} record
   */
  loadInitialValues(record) {
    for (let field in record) {
      let input = document.getElementById(field);
      if (input) {
        input.value = record[field];
      }
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
  constructor(elements, record) {
    let country = record ? record.country :
                  FormAutofillUtils.supportedCountries.find(supported => supported == FormAutofillUtils.DEFAULT_REGION);
    super(elements, record || {country});

    Object.assign(this._elements, {
      addressLevel1Label: this._elements.form.querySelector("#address-level1-container > span"),
      postalCodeLabel: this._elements.form.querySelector("#postal-code-container > span"),
      country: this._elements.form.querySelector("#country"),
    });

    this.populateCountries();
    this.formatForm(country);
    this.attachEventListeners();
  }

  /**
   * Format the form based on country. The address-level1 and postal-code labels
   * should be specific to the given country.
   * @param  {string} country
   */
  formatForm(country) {
    const {addressLevel1Label, postalCodeLabel, fieldsOrder} = FormAutofillUtils.getFormFormat(country);
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
    for (let country of FormAutofillUtils.supportedCountries) {
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
  constructor(elements, record) {
    super(elements, record);
    Object.assign(this._elements, {
      ccNumber: this._elements.form.querySelector("#cc-number"),
      year: this._elements.form.querySelector("#cc-exp-year"),
    });
    this.generateYears();
    this.attachEventListeners();
  }

  generateYears() {
    const count = 11;
    const currentYear = new Date().getFullYear();
    const ccExpYear = this._record && this._record["cc-exp-year"];

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

  handleInput(event) {
    // Clear the error message if cc-number is valid
    if (event.target == this._elements.ccNumber &&
        FormAutofillUtils.isCCNumber(this._elements.ccNumber.value)) {
      this._elements.ccNumber.setCustomValidity("");
    }
    super.handleInput(event);
  }
}
