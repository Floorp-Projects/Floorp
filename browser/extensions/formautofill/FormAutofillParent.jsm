/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements a service used to access storage and communicate with content.
 *
 * A "fields" array is used to communicate with FormAutofillContent. Each item
 * represents a single input field in the content page as well as its
 * @autocomplete properties. The schema is as below. Please refer to
 * FormAutofillContent.js for more details.
 *
 * [
 *   {
 *     section,
 *     addressType,
 *     contactType,
 *     fieldName,
 *     value,
 *     index
 *   },
 *   {
 *     // ...
 *   }
 * ]
 */

/* exported FormAutofillParent */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillParent"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillPreferences",
                                  "resource://formautofill/FormAutofillPreferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillDoorhanger",
                                  "resource://formautofill/FormAutofillDoorhanger.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MasterPassword",
                                  "resource://formautofill/MasterPassword.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const {
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
  CREDITCARDS_COLLECTION_NAME,
} = FormAutofillUtils;

function FormAutofillParent() {
  // Lazily load the storage JSM to avoid disk I/O until absolutely needed.
  // Once storage is loaded we need to update saved field names and inform content processes.
  XPCOMUtils.defineLazyGetter(this, "profileStorage", () => {
    let {profileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});
    log.debug("Loading profileStorage");

    profileStorage.initialize().then(() => {
      // Update the saved field names to compute the status and update child processes.
      this._updateSavedFieldNames();
    });

    return profileStorage;
  });
}

FormAutofillParent.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  /**
   * Cache of the Form Autofill status (considering preferences and storage).
   */
  _active: null,

  /**
   * Initializes ProfileStorage and registers the message handler.
   */
  async init() {
    Services.obs.addObserver(this, "sync-pane-loaded");
    Services.ppmm.addMessageListener("FormAutofill:InitStorage", this);
    Services.ppmm.addMessageListener("FormAutofill:GetRecords", this);
    Services.ppmm.addMessageListener("FormAutofill:SaveAddress", this);
    Services.ppmm.addMessageListener("FormAutofill:SaveCreditCard", this);
    Services.ppmm.addMessageListener("FormAutofill:RemoveAddresses", this);
    Services.ppmm.addMessageListener("FormAutofill:RemoveCreditCards", this);
    Services.ppmm.addMessageListener("FormAutofill:OpenPreferences", this);
    Services.ppmm.addMessageListener("FormAutofill:GetDecryptedString", this);
    Services.mm.addMessageListener("FormAutofill:OnFormSubmit", this);

    // Observing the pref and storage changes
    Services.prefs.addObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.prefs.addObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
    Services.obs.addObserver(this, "formautofill-storage-changed");
  },

  observe(subject, topic, data) {
    log.debug("observe:", topic, "with data:", data);
    switch (topic) {
      case "sync-pane-loaded": {
        let formAutofillPreferences = new FormAutofillPreferences();
        let document = subject.document;
        let prefGroup = formAutofillPreferences.init(document);
        let parentNode = document.getElementById("passwordsGroup");
        let insertBeforeNode = document.getElementById("masterPasswordRow");
        parentNode.insertBefore(prefGroup, insertBeforeNode);
        break;
      }

      case "nsPref:changed": {
        // Observe pref changes and update _active cache if status is changed.
        this._updateStatus();
        break;
      }

      case "formautofill-storage-changed": {
        // Early exit if only metadata is changed
        if (data == "notifyUsed") {
          break;
        }

        this._updateSavedFieldNames();
        break;
      }

      default: {
        throw new Error(`FormAutofillParent: Unexpected topic observed: ${topic}`);
      }
    }
  },

  /**
   * Broadcast the status to frames when the form autofill status changes.
   */
  _onStatusChanged() {
    log.debug("_onStatusChanged: Status changed to", this._active);
    Services.ppmm.broadcastAsyncMessage("FormAutofill:enabledStatus", this._active);
    // Sync process data autofillEnabled to make sure the value up to date
    // no matter when the new content process is initialized.
    Services.ppmm.initialProcessData.autofillEnabled = this._active;
  },

  /**
   * Query preference and storage status to determine the overall status of the
   * form autofill feature.
   *
   * @returns {boolean} whether form autofill is active (enabled and has data)
   */
  _computeStatus() {
    const savedFieldNames = Services.ppmm.initialProcessData.autofillSavedFieldNames;

    return (Services.prefs.getBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF) ||
           Services.prefs.getBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF)) &&
           savedFieldNames &&
           savedFieldNames.size > 0;
  },

  /**
   * Update the status and trigger _onStatusChanged, if necessary.
   */
  _updateStatus() {
    let wasActive = this._active;
    this._active = this._computeStatus();
    if (this._active !== wasActive) {
      this._onStatusChanged();
    }
  },

  /**
   * Handles the message coming from FormAutofillContent.
   *
   * @param   {string} message.name The name of the message.
   * @param   {object} message.data The data of the message.
   * @param   {nsIFrameMessageManager} message.target Caller's message manager.
   */
  async receiveMessage({name, data, target}) {
    switch (name) {
      case "FormAutofill:InitStorage": {
        this.profileStorage.initialize();
        break;
      }
      case "FormAutofill:GetRecords": {
        this._getRecords(data, target);
        break;
      }
      case "FormAutofill:SaveAddress": {
        if (data.guid) {
          this.profileStorage.addresses.update(data.guid, data.address);
        } else {
          this.profileStorage.addresses.add(data.address);
        }
        break;
      }
      case "FormAutofill:SaveCreditCard": {
        await this.profileStorage.creditCards.normalizeCCNumberFields(data.creditcard);
        this.profileStorage.creditCards.add(data.creditcard);
        break;
      }
      case "FormAutofill:RemoveAddresses": {
        data.guids.forEach(guid => this.profileStorage.addresses.remove(guid));
        break;
      }
      case "FormAutofill:RemoveCreditCards": {
        data.guids.forEach(guid => this.profileStorage.creditCards.remove(guid));
        break;
      }
      case "FormAutofill:OnFormSubmit": {
        this._onFormSubmit(data, target);
        break;
      }
      case "FormAutofill:OpenPreferences": {
        const win = RecentWindow.getMostRecentBrowserWindow();
        win.openPreferences("panePrivacy", {origin: "autofillFooter"});
        break;
      }
      case "FormAutofill:GetDecryptedString": {
        let {cipherText, reauth} = data;
        let string;
        try {
          string = await MasterPassword.decrypt(cipherText, reauth);
        } catch (e) {
          if (e.result != Cr.NS_ERROR_ABORT) {
            throw e;
          }
          log.warn("User canceled master password entry");
        }
        target.sendAsyncMessage("FormAutofill:DecryptedString", string);
        break;
      }
    }
  },

  /**
   * Uninitializes FormAutofillParent. This is for testing only.
   *
   * @private
   */
  _uninit() {
    this.profileStorage._saveImmediately();

    Services.ppmm.removeMessageListener("FormAutofill:InitStorage", this);
    Services.ppmm.removeMessageListener("FormAutofill:GetRecords", this);
    Services.ppmm.removeMessageListener("FormAutofill:SaveAddress", this);
    Services.ppmm.removeMessageListener("FormAutofill:SaveCreditCard", this);
    Services.ppmm.removeMessageListener("FormAutofill:RemoveAddresses", this);
    Services.ppmm.removeMessageListener("FormAutofill:RemoveCreditCards", this);
    Services.obs.removeObserver(this, "sync-pane-loaded");
    Services.prefs.removeObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.prefs.removeObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
  },

  /**
   * Get the records from profile store and return results back to content
   * process. It will decrypt the credit card number and append
   * "cc-number-decrypted" to each record if MasterPassword isn't set.
   *
   * @private
   * @param  {string} data.collectionName
   *         The name used to specify which collection to retrieve records.
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched records.
   * @param  {string} data.info
   *         The input autocomplete property's information.
   * @param  {nsIFrameMessageManager} target
   *         Content's message manager.
   */
  async _getRecords({collectionName, searchString, info}, target) {
    let collection = this.profileStorage[collectionName];
    if (!collection) {
      target.sendAsyncMessage("FormAutofill:Records", []);
      return;
    }

    let recordsInCollection = collection.getAll();
    if (!info || !info.fieldName || !recordsInCollection.length) {
      target.sendAsyncMessage("FormAutofill:Records", recordsInCollection);
      return;
    }

    let isCCAndMPEnabled = collectionName == CREDITCARDS_COLLECTION_NAME && MasterPassword.isEnabled;
    // We don't filter "cc-number" when MasterPassword is set.
    if (isCCAndMPEnabled && info.fieldName == "cc-number") {
      recordsInCollection = recordsInCollection.filter(record => !!record["cc-number"]);
      target.sendAsyncMessage("FormAutofill:Records", recordsInCollection);
      return;
    }

    let records = [];
    let lcSearchString = searchString.toLowerCase();

    for (let record of recordsInCollection) {
      let fieldValue = record[info.fieldName];
      if (!fieldValue) {
        continue;
      }

      // Cache the decrypted "cc-number" in each record for content to preview
      // when MasterPassword isn't set.
      if (!isCCAndMPEnabled && record["cc-number-encrypted"]) {
        record["cc-number-decrypted"] = await MasterPassword.decrypt(record["cc-number-encrypted"]);
      }

      // Filter "cc-number" based on the decrypted one.
      if (info.fieldName == "cc-number") {
        fieldValue = record["cc-number-decrypted"];
      }

      if (!lcSearchString || String(fieldValue).toLowerCase().startsWith(lcSearchString)) {
        records.push(record);
      }
    }

    target.sendAsyncMessage("FormAutofill:Records", records);
  },

  _updateSavedFieldNames() {
    log.debug("_updateSavedFieldNames");
    if (!Services.ppmm.initialProcessData.autofillSavedFieldNames) {
      Services.ppmm.initialProcessData.autofillSavedFieldNames = new Set();
    } else {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.clear();
    }

    ["addresses", "creditCards"].forEach(c => {
      this.profileStorage[c].getAll().forEach((record) => {
        Object.keys(record).forEach((fieldName) => {
          if (!record[fieldName]) {
            return;
          }
          Services.ppmm.initialProcessData.autofillSavedFieldNames.add(fieldName);
        });
      });
    });

    // Remove the internal guid and metadata fields.
    this.profileStorage.INTERNAL_FIELDS.forEach((fieldName) => {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.delete(fieldName);
    });

    Services.ppmm.broadcastAsyncMessage("FormAutofill:savedFieldNames",
                                        Services.ppmm.initialProcessData.autofillSavedFieldNames);
    this._updateStatus();
  },

  _onAddressSubmit(address, target, timeStartedFillingMS) {
    if (address.guid) {
      // Avoid updating the fields that users don't modify.
      let originalAddress = this.profileStorage.addresses.get(address.guid);
      for (let field in address.record) {
        if (address.untouchedFields.includes(field) && originalAddress[field]) {
          address.record[field] = originalAddress[field];
        }
      }

      if (!this.profileStorage.addresses.mergeIfPossible(address.guid, address.record)) {
        this._recordFormFillingTime("address", "autofill-update", timeStartedFillingMS);

        FormAutofillDoorhanger.show(target, "update").then((state) => {
          let changedGUIDs = this.profileStorage.addresses.mergeToStorage(address.record);
          switch (state) {
            case "create":
              if (!changedGUIDs.length) {
                changedGUIDs.push(this.profileStorage.addresses.add(address.record));
              }
              break;
            case "update":
              if (!changedGUIDs.length) {
                this.profileStorage.addresses.update(address.guid, address.record, true);
                changedGUIDs.push(address.guid);
              } else {
                this.profileStorage.addresses.remove(address.guid);
              }
              break;
          }
          changedGUIDs.forEach(guid => this.profileStorage.addresses.notifyUsed(guid));
        });
        // Address should be updated
        Services.telemetry.scalarAdd("formautofill.addresses.fill_type_autofill_update", 1);
        return;
      }
      this._recordFormFillingTime("address", "autofill", timeStartedFillingMS);
      this.profileStorage.addresses.notifyUsed(address.guid);
      // Address is merged successfully
      Services.telemetry.scalarAdd("formautofill.addresses.fill_type_autofill", 1);
    } else {
      let changedGUIDs = this.profileStorage.addresses.mergeToStorage(address.record);
      if (!changedGUIDs.length) {
        changedGUIDs.push(this.profileStorage.addresses.add(address.record));
      }
      changedGUIDs.forEach(guid => this.profileStorage.addresses.notifyUsed(guid));
      this._recordFormFillingTime("address", "manual", timeStartedFillingMS);

      // Show first time use doorhanger
      if (Services.prefs.getBoolPref("extensions.formautofill.firstTimeUse")) {
        Services.prefs.setBoolPref("extensions.formautofill.firstTimeUse", false);
        FormAutofillDoorhanger.show(target, "firstTimeUse").then((state) => {
          if (state !== "open-pref") {
            return;
          }

          target.ownerGlobal.openPreferences("panePrivacy",
                                             {origin: "autofillDoorhanger"});
        });
      } else {
        // We want to exclude the first time form filling.
        Services.telemetry.scalarAdd("formautofill.addresses.fill_type_manual", 1);
      }
    }
  },

  async _onCreditCardSubmit(creditCard, target) {
    // We'll show the credit card doorhanger if:
    //   - User applys autofill and changed
    //   - User fills form manually
    if (creditCard.guid &&
        Object.keys(creditCard.record).every(key => creditCard.untouchedFields.includes(key))) {
      // Add probe to record credit card autofill(without modification).
      Services.telemetry.scalarAdd("formautofill.creditCards.fill_type_autofill", 1);
      return;
    }

    // Add the probe to record credit card manual filling or autofill but modified case.
    let ccScalar = creditCard.guid ? "formautofill.creditCards.fill_type_autofill_modified" :
                                     "formautofill.creditCards.fill_type_manual";
    Services.telemetry.scalarAdd(ccScalar, 1);

    let state = await FormAutofillDoorhanger.show(target, "creditCard");
    if (state == "cancel") {
      return;
    }

    if (state == "disable") {
      Services.prefs.setBoolPref("extensions.formautofill.creditCards.enabled", false);
      return;
    }

    await this.profileStorage.creditCards.normalizeCCNumberFields(creditCard.record);
    this.profileStorage.creditCards.add(creditCard.record);
  },

  _onFormSubmit(data, target) {
    let {profile: {address, creditCard}, timeStartedFillingMS} = data;

    if (address) {
      this._onAddressSubmit(address, target, timeStartedFillingMS);
    }
    if (creditCard) {
      this._onCreditCardSubmit(creditCard, target);
    }
  },
  /**
   * Set the probes for the filling time with specific filling type and form type.
   *
   * @private
   * @param  {string} formType
   *         3 type of form (address/creditcard/address-creditcard).
   * @param  {string} fillingType
   *         3 filling type (manual/autofill/autofill-update).
   * @param  {int} startedFillingMS
   *         Time that form started to filling in ms.
   */
  _recordFormFillingTime(formType, fillingType, startedFillingMS) {
    let histogram = Services.telemetry.getKeyedHistogramById("FORM_FILLING_REQUIRED_TIME_MS");
    histogram.add(`${formType}-${fillingType}`, Date.now() - startedFillingMS);
  },
};
