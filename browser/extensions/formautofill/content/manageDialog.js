/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported ManageAddresses, ManageCreditCards */

"use strict";

const EDIT_ADDRESS_URL = "chrome://formautofill/content/editAddress.xhtml";
const EDIT_CREDIT_CARD_URL = "chrome://formautofill/content/editCreditCard.xhtml";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://formautofill/FormAutofillUtils.jsm");

ChromeUtils.defineModuleGetter(this, "CreditCard",
                               "resource://gre/modules/CreditCard.jsm");
ChromeUtils.defineModuleGetter(this, "formAutofillStorage",
                               "resource://formautofill/FormAutofillStorage.jsm");
ChromeUtils.defineModuleGetter(this, "MasterPassword",
                               "resource://formautofill/MasterPassword.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, "manageAddresses");

class ManageRecords {
  constructor(subStorageName, elements) {
    this._storageInitPromise = formAutofillStorage.initialize();
    this._subStorageName = subStorageName;
    this._elements = elements;
    this._newRequest = false;
    this._isLoadingRecords = false;
    this.prefWin = window.opener;
    this.localizeDocument();
    window.addEventListener("DOMContentLoaded", this, {once: true});
  }

  async init() {
    await this.loadRecords();
    this.attachEventListeners();
    // For testing only: Notify when the dialog is ready for interaction
    window.dispatchEvent(new CustomEvent("FormReady"));
  }

  uninit() {
    log.debug("uninit");
    this.detachEventListeners();
    this._elements = null;
  }

  localizeDocument() {
    document.documentElement.style.minWidth = FormAutofillUtils.stringBundle.GetStringFromName("manageDialogsWidth");
    FormAutofillUtils.localizeMarkup(document);
  }

  /**
   * Get the selected options on the addresses element.
   *
   * @returns {array<DOMElement>}
   */
  get _selectedOptions() {
    return Array.from(this._elements.records.selectedOptions);
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
   * Load records and render them. This function is a wrapper for _loadRecords
   * to ensure any reentrant will be handled well.
   */
  async loadRecords() {
    // This function can be early returned when there is any reentrant happends.
    // "_newRequest" needs to be set to ensure all changes will be applied.
    if (this._isLoadingRecords) {
      this._newRequest = true;
      return;
    }
    this._isLoadingRecords = true;

    await this._loadRecords();

    // _loadRecords should be invoked again if there is any multiple entrant
    // during running _loadRecords(). This step ensures that the latest request
    // still is applied.
    while (this._newRequest) {
      this._newRequest = false;
      await this._loadRecords();
    }
    this._isLoadingRecords = false;

    // For testing only: Notify when records are loaded
    this._elements.records.dispatchEvent(new CustomEvent("RecordsLoaded"));
  }

  async _loadRecords() {
    let storage = await this.getStorage();
    let records = storage.getAll();
    // Sort by last modified time starting with most recent
    records.sort((a, b) => b.timeLastModified - a.timeLastModified);
    await this.renderRecordElements(records);
    this.updateButtonsStates(this._selectedOptions.length);
  }

  /**
   * Render the records onto the page while maintaining selected options if
   * they still exist.
   *
   * @param  {array<object>} records
   */
  async renderRecordElements(records) {
    let selectedGuids = this._selectedOptions.map(option => option.value);
    this.clearRecordElements();
    for (let record of records) {
      let option = new Option(await this.getLabel(record),
                              record.guid,
                              false,
                              selectedGuids.includes(record.guid));
      option.record = record;
      this._elements.records.appendChild(option);
    }
  }

  /**
   * Remove all existing record elements.
   */
  clearRecordElements() {
    let parent = this._elements.records;
    while (parent.lastChild) {
      parent.removeChild(parent.lastChild);
    }
  }

  /**
   * Remove records by selected options.
   *
   * @param  {array<DOMElement>} options
   */
  async removeRecords(options) {
    let storage = await this.getStorage();
    // Pause listening to storage change event to avoid triggering `loadRecords`
    // when removing records
    Services.obs.removeObserver(this, "formautofill-storage-changed");

    for (let option of options) {
      storage.remove(option.value);
      option.remove();
    }
    this.updateButtonsStates(this._selectedOptions);

    // Resume listening to storage change event
    Services.obs.addObserver(this, "formautofill-storage-changed");
    // For testing only: notify record(s) has been removed
    this._elements.records.dispatchEvent(new CustomEvent("RecordsRemoved"));
  }

  /**
   * Enable/disable the Edit and Remove buttons based on number of selected
   * options.
   *
   * @param  {number} selectedCount
   */
  updateButtonsStates(selectedCount) {
    log.debug("updateButtonsStates:", selectedCount);
    if (selectedCount == 0) {
      this._elements.edit.setAttribute("disabled", "disabled");
      this._elements.remove.setAttribute("disabled", "disabled");
    } else if (selectedCount == 1) {
      this._elements.edit.removeAttribute("disabled");
      this._elements.remove.removeAttribute("disabled");
    } else if (selectedCount > 1) {
      this._elements.edit.setAttribute("disabled", "disabled");
      this._elements.remove.removeAttribute("disabled");
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
      case "change": {
        this.updateButtonsStates(this._selectedOptions.length);
        break;
      }
      case "unload": {
        this.uninit();
        break;
      }
      case "keypress": {
        this.handleKeyPress(event);
        break;
      }
      case "contextmenu": {
        event.preventDefault();
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
    if (event.target == this._elements.remove) {
      this.removeRecords(this._selectedOptions);
    } else if (event.target == this._elements.add) {
      this.openEditDialog();
    } else if (event.target == this._elements.edit ||
               event.target.parentNode == this._elements.records && event.detail > 1) {
      this.openEditDialog(this._selectedOptions[0].record);
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
    if (event.keyCode == KeyEvent.DOM_VK_DELETE) {
      this.removeRecords(this._selectedOptions);
    }
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "formautofill-storage-changed": {
        this.loadRecords();
      }
    }
  }

  /**
   * Attach event listener
   */
  attachEventListeners() {
    window.addEventListener("unload", this, {once: true});
    window.addEventListener("keypress", this);
    window.addEventListener("contextmenu", this);
    this._elements.records.addEventListener("change", this);
    this._elements.records.addEventListener("click", this);
    this._elements.controlsContainer.addEventListener("click", this);
    Services.obs.addObserver(this, "formautofill-storage-changed");
  }

  /**
   * Remove event listener
   */
  detachEventListeners() {
    window.removeEventListener("keypress", this);
    window.removeEventListener("contextmenu", this);
    this._elements.records.removeEventListener("change", this);
    this._elements.records.removeEventListener("click", this);
    this._elements.controlsContainer.removeEventListener("click", this);
    Services.obs.removeObserver(this, "formautofill-storage-changed");
  }
}

class ManageAddresses extends ManageRecords {
  constructor(elements) {
    super("addresses", elements);
    elements.add.setAttribute("searchkeywords", FormAutofillUtils.EDIT_ADDRESS_KEYWORDS
                                                  .map(key => FormAutofillUtils.stringBundle.GetStringFromName(key))
                                                  .join("\n"));
  }

  /**
   * Open the edit address dialog to create/edit an address.
   *
   * @param  {object} address [optional]
   */
  openEditDialog(address) {
    this.prefWin.gSubDialog.open(EDIT_ADDRESS_URL, null, address, "novalidate");
  }

  getLabel(address) {
    return FormAutofillUtils.getAddressLabel(address);
  }
}

class ManageCreditCards extends ManageRecords {
  constructor(elements) {
    super("creditCards", elements);
    elements.add.setAttribute("searchkeywords", FormAutofillUtils.EDIT_CREDITCARD_KEYWORDS
                                                  .map(key => FormAutofillUtils.stringBundle.GetStringFromName(key))
                                                  .join("\n"));
    this._hasMasterPassword = MasterPassword.isEnabled;
    this._isDecrypted = false;
    if (this._hasMasterPassword) {
      elements.showHideCreditCards.setAttribute("hidden", true);
    }
  }

  /**
   * Open the edit address dialog to create/edit a credit card.
   *
   * @param  {object} creditCard [optional]
   */
  async openEditDialog(creditCard) {
    // If master password is set, ask for password if user is trying to edit an
    // existing credit card.
    if (!creditCard || !this._hasMasterPassword || await MasterPassword.ensureLoggedIn(true)) {
      let decryptedCCNumObj = {};
      if (creditCard) {
        decryptedCCNumObj["cc-number"] = await MasterPassword.decrypt(creditCard["cc-number-encrypted"]);
      }
      let decryptedCreditCard = Object.assign({}, creditCard, decryptedCCNumObj);
      this.prefWin.gSubDialog.open(EDIT_CREDIT_CARD_URL, "resizable=no", decryptedCreditCard);
    }
  }

  /**
   * Get credit card display label. It should display masked numbers and the
   * cardholder's name, separated by a comma. If `showCreditCards` is set to
   * true, decrypted credit card numbers are shown instead.
   *
   * @param {object} creditCard
   * @param {boolean} showCreditCards [optional]
   * @returns {string}
   */
  async getLabel(creditCard, showCreditCards = false) {
    let cardObj = new CreditCard({
      encryptedNumber: creditCard["cc-number-encrypted"],
      number: creditCard["cc-number"],
      name: creditCard["cc-name"],
    });
    return cardObj.getLabel({showNumbers: showCreditCards});
  }

  async toggleShowHideCards(options) {
    this._isDecrypted = !this._isDecrypted;
    this.updateShowHideButtonState();
    await this.updateLabels(options, this._isDecrypted);
  }

  async updateLabels(options, isDecrypted) {
    for (let option of options) {
      option.text = await this.getLabel(option.record, isDecrypted);
    }
    // For testing only: Notify when credit cards labels have been updated
    this._elements.records.dispatchEvent(new CustomEvent("LabelsUpdated"));
  }

  async renderRecordElements(records) {
    // Revert back to encrypted form when re-rendering happens
    this._isDecrypted = false;
    await super.renderRecordElements(records);
  }

  updateButtonsStates(selectedCount) {
    this.updateShowHideButtonState();
    super.updateButtonsStates(selectedCount);
  }

  updateShowHideButtonState() {
    if (this._elements.records.length) {
      this._elements.showHideCreditCards.removeAttribute("disabled");
    } else {
      this._elements.showHideCreditCards.setAttribute("disabled", true);
    }
    this._elements.showHideCreditCards.textContent =
      this._isDecrypted ? FormAutofillUtils.stringBundle.GetStringFromName("hideCreditCardsBtnLabel") :
                          FormAutofillUtils.stringBundle.GetStringFromName("showCreditCardsBtnLabel");
  }

  handleClick(event) {
    if (event.target == this._elements.showHideCreditCards) {
      this.toggleShowHideCards(this._elements.records.options);
    }
    super.handleClick(event);
  }
}
