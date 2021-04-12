/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported EditAddressDialog, EditCreditCardDialog */
/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

"use strict";

// eslint-disable-next-line no-unused-vars
const { FormAutofill } = ChromeUtils.import(
  "resource://autofill/FormAutofill.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "formAutofillStorage",
  "resource://autofill/FormAutofillStorage.jsm"
);

class AutofillEditDialog {
  constructor(subStorageName, elements, record) {
    this._storageInitPromise = formAutofillStorage.initialize();
    this._subStorageName = subStorageName;
    this._elements = elements;
    this._record = record;
    this.localizeDocument();
    window.addEventListener("DOMContentLoaded", this, { once: true });
  }

  async init() {
    this.updateSaveButtonState();
    this.attachEventListeners();
    // For testing only: signal to tests that the dialog is ready for testing.
    // This is likely no longer needed since retrieving from storage is fully
    // handled in manageDialog.js now.
    window.dispatchEvent(new CustomEvent("FormReady"));
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
      await storage.update(guid, record);
    } else {
      await storage.add(record);
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
        if (
          !(event.target instanceof HTMLInputElement) &&
          !(event.target instanceof HTMLTextAreaElement)
        ) {
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
    this.updateSaveButtonState();
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

  updateSaveButtonState() {
    // Toggle disabled attribute on the save button based on
    // whether the form is filled or empty.
    if (!Object.keys(this._elements.fieldContainer.buildFormObject()).length) {
      this._elements.save.setAttribute("disabled", true);
    } else {
      this._elements.save.removeAttribute("disabled");
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
    super("addresses", elements, record);
  }

  localizeDocument() {
    if (this._record?.guid) {
      this._elements.title.dataset.localization = "editAddressTitle";
    }
  }

  async handleSubmit() {
    await this.saveRecord(
      this._elements.fieldContainer.buildFormObject(),
      this._record ? this._record.guid : null
    );
    window.close();
  }
}

class EditCreditCardDialog extends AutofillEditDialog {
  constructor(elements, record) {
    elements.fieldContainer._elements.billingAddress.disabled = true;
    super("creditCards", elements, record);
    elements.fieldContainer._elements.ccNumber.addEventListener(
      "blur",
      this._onCCNumberFieldBlur.bind(this)
    );
    if (record) {
      Services.telemetry.recordEvent("creditcard", "show_entry", "manage");
    }
  }

  _onCCNumberFieldBlur() {
    let elem = this._elements.fieldContainer._elements.ccNumber;
    this._elements.fieldContainer.updateCustomValidity(elem);
  }

  localizeDocument() {
    if (this._record?.guid) {
      this._elements.title.dataset.localization = "editCreditCardTitle";
    }
  }

  async handleSubmit() {
    let creditCard = this._elements.fieldContainer.buildFormObject();
    if (!this._elements.fieldContainer._elements.form.reportValidity()) {
      return;
    }

    try {
      await this.saveRecord(
        creditCard,
        this._record ? this._record.guid : null
      );

      if (this._record?.guid) {
        Services.telemetry.recordEvent("creditcard", "edit", "manage");
      } else {
        Services.telemetry.recordEvent("creditcard", "add", "manage");
      }

      window.close();
    } catch (ex) {
      Cu.reportError(ex);
    }
  }
}
