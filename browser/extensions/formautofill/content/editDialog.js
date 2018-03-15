/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported EditAddressDialog, EditCreditCardDialog */
/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

"use strict";

ChromeUtils.import("resource://formautofill/FormAutofillUtils.jsm");

ChromeUtils.defineModuleGetter(this, "formAutofillStorage",
                               "resource://formautofill/FormAutofillStorage.jsm");
ChromeUtils.defineModuleGetter(this, "MasterPassword",
                               "resource://formautofill/MasterPassword.jsm");

class AutofillEditDialog {
  constructor(subStorageName, elements, record) {
    this._storageInitPromise = formAutofillStorage.initialize();
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
   * Get storage and ensure it has been initialized.
   * @returns {object}
   */
  async getStorage() {
    await this._storageInitPromise;
    return formAutofillStorage[this._subStorageName];
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
      case "contextmenu": {
        if (!(event.target instanceof HTMLInputElement) &&
            !(event.target instanceof HTMLTextAreaElement)) {
          event.preventDefault();
        }
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
    if (Object.keys(this._elements.fieldContainer.buildFormObject()).length == 0) {
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
    window.addEventListener("contextmenu", this);
    this._elements.controlsContainer.addEventListener("click", this);
    document.addEventListener("input", this);
  }

  // An interface to be inherited.
  localizeDocument() {}
}

class EditAddressDialog extends AutofillEditDialog {
  constructor(elements, record) {
    let country = record ? record.country :
                  FormAutofillUtils.supportedCountries.find(supported => supported == FormAutofillUtils.DEFAULT_REGION);
    super("addresses", elements, record || {country});
  }

  localizeDocument() {
    if (this._record) {
      this._elements.title.dataset.localization = "editAddressTitle";
    }
    FormAutofillUtils.localizeMarkup(document);
  }

  async handleSubmit() {
    await this.saveRecord(this._elements.fieldContainer.buildFormObject(), this._record ? this._record.guid : null);
    window.close();
  }
}

class EditCreditCardDialog extends AutofillEditDialog {
  constructor(elements, record) {
    super("creditCards", elements, record);
  }

  localizeDocument() {
    if (this._record) {
      this._elements.title.dataset.localization = "editCreditCardTitle";
    }
    FormAutofillUtils.localizeMarkup(document);
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
    let creditCard = this._elements.fieldContainer.buildFormObject();
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
}
