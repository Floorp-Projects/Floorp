/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported ManageAddresses, ManageCreditCards */

"use strict";

const EDIT_ADDRESS_URL = "chrome://formautofill/content/editAddress.xhtml";
const EDIT_CREDIT_CARD_URL =
  "chrome://formautofill/content/editCreditCard.xhtml";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { FormAutofill } = ChromeUtils.import(
  "resource://autofill/FormAutofill.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "CreditCard",
  "resource://gre/modules/CreditCard.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "formAutofillStorage",
  "resource://autofill/FormAutofillStorage.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FormAutofillUtils",
  "resource://autofill/FormAutofillUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "OSKeyStore",
  "resource://gre/modules/OSKeyStore.jsm"
);

const lazy = {};
XPCOMUtils.defineLazyGetter(
  lazy,
  "l10n",
  () =>
    new Localization([
      "browser/preferences/formAutofill.ftl",
      "branding/brand.ftl",
    ])
);

this.log = null;
XPCOMUtils.defineLazyGetter(this, "log", () =>
  FormAutofill.defineLogGetter(this, "manageAddresses")
);

class ManageRecords {
  constructor(subStorageName, elements) {
    this._storageInitPromise = formAutofillStorage.initialize();
    this._subStorageName = subStorageName;
    this._elements = elements;
    this._newRequest = false;
    this._isLoadingRecords = false;
    this.prefWin = window.opener;
    window.addEventListener("DOMContentLoaded", this, { once: true });
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
    let records = await storage.getAll();
    // Sort by last used time starting with most recent
    records.sort((a, b) => {
      let aLastUsed = a.timeLastUsed || a.timeLastModified;
      let bLastUsed = b.timeLastUsed || b.timeLastModified;
      return bLastUsed - aLastUsed;
    });
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
      let { id, args, raw } = await this.getLabelInfo(record);
      let option = new Option(
        raw ?? "",
        record.guid,
        false,
        selectedGuids.includes(record.guid)
      );
      if (id) {
        document.l10n.setAttributes(option, id, args);
      }

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
    } else if (
      event.target == this._elements.edit ||
      (event.target.parentNode == this._elements.records && event.detail > 1)
    ) {
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
    window.addEventListener("unload", this, { once: true });
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
    elements.add.setAttribute(
      "search-l10n-ids",
      FormAutofillUtils.EDIT_ADDRESS_L10N_IDS.join(",")
    );
  }

  /**
   * Open the edit address dialog to create/edit an address.
   *
   * @param  {object} address [optional]
   */
  openEditDialog(address) {
    this.prefWin.gSubDialog.open(EDIT_ADDRESS_URL, undefined, {
      record: address,
      // Don't validate in preferences since it's fine for fields to be missing
      // for autofill purposes. For PaymentRequest addresses get more validation.
      noValidate: true,
    });
  }

  getLabelInfo(address) {
    return { raw: FormAutofillUtils.getAddressLabel(address) };
  }
}

class ManageCreditCards extends ManageRecords {
  constructor(elements) {
    super("creditCards", elements);
    elements.add.setAttribute(
      "search-l10n-ids",
      FormAutofillUtils.EDIT_CREDITCARD_L10N_IDS.join(",")
    );

    Services.telemetry.recordEvent("creditcard", "show", "manage");

    this._isDecrypted = false;
  }

  /**
   * Open the edit address dialog to create/edit a credit card.
   *
   * @param  {object} creditCard [optional]
   */
  async openEditDialog(creditCard) {
    // Ask for reauth if user is trying to edit an existing credit card.
    if (creditCard) {
      const reauthPasswordPromptMessage = await lazy.l10n.formatValue(
        "autofill-edit-card-password-prompt"
      );
      const loggedIn = await FormAutofillUtils.ensureLoggedIn(
        reauthPasswordPromptMessage
      );
      if (!loggedIn.authenticated) {
        return;
      }
    }

    let decryptedCCNumObj = {};
    if (creditCard && creditCard["cc-number-encrypted"]) {
      try {
        decryptedCCNumObj["cc-number"] = await OSKeyStore.decrypt(
          creditCard["cc-number-encrypted"]
        );
      } catch (ex) {
        if (ex.result == Cr.NS_ERROR_ABORT) {
          // User shouldn't be ask to reauth here, but it could happen.
          // Return here and skip opening the dialog.
          return;
        }
        // We've got ourselves a real error.
        // Recover from encryption error so the user gets a chance to re-enter
        // unencrypted credit card number.
        decryptedCCNumObj["cc-number"] = "";
        Cu.reportError(ex);
      }
    }
    let decryptedCreditCard = Object.assign({}, creditCard, decryptedCCNumObj);
    this.prefWin.gSubDialog.open(
      EDIT_CREDIT_CARD_URL,
      { features: "resizable=no" },
      {
        record: decryptedCreditCard,
      }
    );
  }

  /**
   * Get credit card display label. It should display masked numbers and the
   * cardholder's name, separated by a comma.
   *
   * @param {object} creditCard
   * @returns {Promise<string>}
   */
  async getLabelInfo(creditCard) {
    // The card type is displayed visually using an image. For a11y, we need
    // to expose it as text. We do this using aria-label. However,
    // aria-label overrides the text content, so we must include that also.
    // Since the text content is generated by Fluent, aria-label must be
    // generated by Fluent also.
    const type = creditCard["cc-type"];
    const typeL10nId = CreditCard.getNetworkL10nId(type);
    const typeName = typeL10nId
      ? await document.l10n.formatValue(typeL10nId)
      : type ?? ""; // Unknown card type
    return CreditCard.getLabelInfo({
      name: creditCard["cc-name"],
      number: creditCard["cc-number"],
      month: creditCard["cc-exp-month"],
      year: creditCard["cc-exp-year"],
      type: typeName,
    });
  }

  async renderRecordElements(records) {
    // Revert back to encrypted form when re-rendering happens
    this._isDecrypted = false;
    // Display third-party card icons when possible
    this._elements.records.classList.toggle(
      "branded",
      AppConstants.MOZILLA_OFFICIAL
    );
    await super.renderRecordElements(records);

    let options = this._elements.records.options;
    for (let option of options) {
      let record = option.record;
      if (record && record["cc-type"]) {
        option.setAttribute("cc-type", record["cc-type"]);
      } else {
        option.removeAttribute("cc-type");
      }
    }
  }

  async removeRecords(options) {
    await super.removeRecords(options);
    for (let i = 0; i < options.length; i++) {
      Services.telemetry.recordEvent("creditcard", "delete", "manage");
    }
  }

  updateButtonsStates(selectedCount) {
    super.updateButtonsStates(selectedCount);
  }

  handleClick(event) {
    super.handleClick(event);
  }
}
