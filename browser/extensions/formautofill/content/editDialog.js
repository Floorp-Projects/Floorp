/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported EditAddress, EditCreditCard */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const AUTOFILL_BUNDLE_URI = "chrome://formautofill/locale/formautofill.properties";
const REGIONS_BUNDLE_URI = "chrome://global/locale/regionNames.properties";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "profileStorage",
                                  "resource://formautofill/ProfileStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MasterPassword",
                                  "resource://formautofill/MasterPassword.jsm");

class EditDialog {
  constructor(subStorageName, elements, record) {
    this._storageInitPromise = profileStorage.initialize();
    this._subStorageName = subStorageName;
    this._elements = elements;
    this._record = record;
    this.localizeDocument();
    window.addEventListener("DOMContentLoaded", this, {once: true});
  }

  async init() {
    if (this._record) {
      await this.loadInitialValues(this._record);
    }
    this.attachEventListeners();
    // For testing only: loadInitialValues for credit card is an async method, and tests
    // need to wait until the values have been filled before editing the fields.
    window.dispatchEvent(new CustomEvent("FormReady"));
  }

  uninit() {
    this.detachEventListeners();
    this._elements = null;
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
    return Array.from(document.forms[0].elements).reduce((obj, input) => {
      if (input.value) {
        obj[input.id] = input.value;
      }
      return obj;
    }, {});
  }

  /**
   * Get storage and ensure it has been initialized.
   * @returns {object}
   */
  async getStorage() {
    await this._storageInitPromise;
    return profileStorage[this._subStorageName];
  }

  /**
   * Asks FormAutofillParent to save or update an record.
   * @param  {object} record
   * @param  {string} guid [optional]
   */
  async saveRecord(record, guid) {
    let storage = await this.getStorage();
    if (guid) {
      storage.update(guid, record);
    } else {
      storage.add(record);
    }
  }

  /**
   * Handle events
   *
   * @param  {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded": {
        this.init();
        break;
      }
      case "unload": {
        this.uninit();
        break;
      }
      case "click": {
        this.handleClick(event);
        break;
      }
      case "input": {
        this.handleInput(event);
        break;
      }
      case "keypress": {
        this.handleKeyPress(event);
        break;
      }
      case "change": {
        this.handleChange(event);
        break;
      }
    }
  }

  /**
   * Handle click events
   *
   * @param  {DOMEvent} event
   */
  handleClick(event) {
    if (event.target == this._elements.cancel) {
      window.close();
    }
    if (event.target == this._elements.save) {
      this.handleSubmit();
    }
  }

  /**
   * Handle input events
   *
   * @param  {DOMEvent} event
   */
  handleInput(event) {
    // Toggle disabled attribute on the save button based on
    // whether the form is filled or empty.
    if (Object.keys(this.buildFormObject()).length == 0) {
      this._elements.save.setAttribute("disabled", true);
    } else {
      this._elements.save.removeAttribute("disabled");
    }
  }

  /**
   * Handle key press events
   *
   * @param  {DOMEvent} event
   */
  handleKeyPress(event) {
    if (event.keyCode == KeyEvent.DOM_VK_ESCAPE) {
      window.close();
    }
  }

  /**
   * Attach event listener
   */
  attachEventListeners() {
    window.addEventListener("keypress", this);
    this._elements.controlsContainer.addEventListener("click", this);
    document.addEventListener("input", this);
  }

  /**
   * Remove event listener
   */
  detachEventListeners() {
    window.removeEventListener("keypress", this);
    this._elements.controlsContainer.removeEventListener("click", this);
    document.removeEventListener("input", this);
  }

  // An interface to be inherited.
  localizeDocument() {}

  // An interface to be inherited.
  handleSubmit(event) {}

  // An interface to be inherited.
  handleChange(event) {}
}

class EditAddress extends EditDialog {
  constructor(elements, record) {
    super("addresses", elements, record);
    this.formatForm(record && record.country);
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
    FormAutofillUtils.localizeMarkup(AUTOFILL_BUNDLE_URI, document);
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

  localizeDocument() {
    if (this._record) {
      this._elements.title.dataset.localization = "editAddressTitle";
    }
    let fragment = document.createDocumentFragment();
    for (let country of FormAutofillUtils.supportedCountries) {
      let option = new Option();
      option.value = country;
      option.dataset.localization = country.toLowerCase();
      fragment.appendChild(option);
    }
    this._elements.country.appendChild(fragment);
    FormAutofillUtils.localizeMarkup(REGIONS_BUNDLE_URI, this._elements.country);
  }

  async handleSubmit() {
    await this.saveRecord(this.buildFormObject(), this._record ? this._record.guid : null);
    window.close();
  }

  handleChange(event) {
    this.formatForm(event.target.value);
  }

  attachEventListeners() {
    this._elements.country.addEventListener("change", this);
    super.attachEventListeners();
  }

  detachEventListeners() {
    this._elements.country.removeEventListener("change", this);
    super.detachEventListeners();
  }
}

class EditCreditCard extends EditDialog {
  constructor(elements, record) {
    super("creditCards", elements, record);
    this.generateYears();
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

  localizeDocument() {
    if (this._record) {
      this._elements.title.dataset.localization = "editCreditCardTitle";
    }
    FormAutofillUtils.localizeMarkup(AUTOFILL_BUNDLE_URI, document);
  }

  /**
   * Decrypt cc-number first and fill the form.
   * @param  {object} creditCard
   */
  async loadInitialValues(creditCard) {
    let decryptedCC = await MasterPassword.decrypt(creditCard["cc-number-encrypted"]);
    super.loadInitialValues(Object.assign({}, creditCard, {"cc-number": decryptedCC}));
  }

  async handleSubmit() {
    let creditCard = this.buildFormObject();
    // Show error on the cc-number field if it's empty or invalid
    if (!FormAutofillUtils.isCCNumber(creditCard["cc-number"])) {
      this._elements.ccNumber.setCustomValidity(true);
      return;
    }

    // TODO: "MasterPassword.ensureLoggedIn" can be removed after the storage
    // APIs are refactored to be async functions (bug 1399367).
    if (await MasterPassword.ensureLoggedIn()) {
      await this.saveRecord(creditCard, this._record ? this._record.guid : null);
    }
    window.close();
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
