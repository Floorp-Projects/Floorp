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

"use strict";

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
var EXPORTED_SYMBOLS = ["formAutofillParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { FormAutofill } = ChromeUtils.import(
  "resource://formautofill/FormAutofill.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  CreditCard: "resource://gre/modules/CreditCard.jsm",
  FormAutofillPreferences:
    "resource://formautofill/FormAutofillPreferences.jsm",
  FormAutofillDoorhanger: "resource://formautofill/FormAutofillDoorhanger.jsm",
  FormAutofillUtils: "resource://formautofill/FormAutofillUtils.jsm",
  OSKeyStore: "resource://formautofill/OSKeyStore.jsm",
});

this.log = null;
FormAutofill.defineLazyLogGetter(this, EXPORTED_SYMBOLS[0]);

const {
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
} = FormAutofill;

const {
  ADDRESSES_COLLECTION_NAME,
  CREDITCARDS_COLLECTION_NAME,
} = FormAutofillUtils;

function FormAutofillParent() {
  // Lazily load the storage JSM to avoid disk I/O until absolutely needed.
  // Once storage is loaded we need to update saved field names and inform content processes.
  XPCOMUtils.defineLazyGetter(this, "formAutofillStorage", () => {
    let { formAutofillStorage } = ChromeUtils.import(
      "resource://formautofill/FormAutofillStorage.jsm"
    );
    log.debug("Loading formAutofillStorage");

    formAutofillStorage.initialize().then(() => {
      // Update the saved field names to compute the status and update child processes.
      this._updateSavedFieldNames();
    });

    return formAutofillStorage;
  });
}

FormAutofillParent.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  /**
   * Cache of the Form Autofill status (considering preferences and storage).
   */
  _active: null,

  /**
   * The status of Form Autofill's initialization.
   */
  _initialized: false,

  /**
   * Exposes the status of Form Autofill's initialization. It can be used to
   * determine whether Form Autofill is available for current users.
   *
   * @returns {boolean} Whether FormAutofillParent is initialized.
   */
  get initialized() {
    return this._initialized;
  },

  /**
   * Initializes FormAutofillStorage and registers the message handler.
   */
  async init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    Services.obs.addObserver(this, "privacy-pane-loaded");
    Services.ppmm.addMessageListener("FormAutofill:InitStorage", this);
    Services.ppmm.addMessageListener("FormAutofill:GetRecords", this);
    Services.ppmm.addMessageListener("FormAutofill:SaveAddress", this);
    Services.ppmm.addMessageListener("FormAutofill:RemoveAddresses", this);
    Services.ppmm.addMessageListener("FormAutofill:OpenPreferences", this);
    Services.mm.addMessageListener("FormAutofill:OnFormSubmit", this);

    // Observing the pref and storage changes
    Services.prefs.addObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.obs.addObserver(this, "formautofill-storage-changed");

    // Only listen to credit card related messages if it is available
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.ppmm.addMessageListener("FormAutofill:SaveCreditCard", this);
      Services.ppmm.addMessageListener("FormAutofill:RemoveCreditCards", this);
      Services.ppmm.addMessageListener("FormAutofill:GetDecryptedString", this);
      Services.prefs.addObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
    }

    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      this.injectElements(win.document);
    }
    Services.wm.addListener(this);
  },

  injectElements(doc) {
    Services.scriptloader.loadSubScript(
      "chrome://formautofill/content/customElements.js",
      doc.ownerGlobal
    );
  },

  onOpenWindow(xulWindow) {
    const win = xulWindow.docShell.domWindow;
    win.addEventListener(
      "load",
      () => {
        if (
          win.document.documentElement.getAttribute("windowtype") ==
          "navigator:browser"
        ) {
          this.injectElements(win.document);
        }
      },
      { once: true }
    );
  },

  onCloseWindow() {},

  observe(subject, topic, data) {
    log.debug("observe:", topic, "with data:", data);
    switch (topic) {
      case "privacy-pane-loaded": {
        let formAutofillPreferences = new FormAutofillPreferences();
        let document = subject.document;
        let prefFragment = formAutofillPreferences.init(document);
        let formAutofillGroupBox = document.getElementById(
          "formAutofillGroupBox"
        );
        formAutofillGroupBox.appendChild(prefFragment);
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
        throw new Error(
          `FormAutofillParent: Unexpected topic observed: ${topic}`
        );
      }
    }
  },

  /**
   * Broadcast the status to frames when the form autofill status changes.
   */
  _onStatusChanged() {
    log.debug("_onStatusChanged: Status changed to", this._active);
    Services.ppmm.sharedData.set("FormAutofill:enabled", this._active);
    // Sync autofill enabled to make sure the value is up-to-date
    // no matter when the new content process is initialized.
    Services.ppmm.sharedData.flush();
  },

  /**
   * Query preference and storage status to determine the overall status of the
   * form autofill feature.
   *
   * @returns {boolean} whether form autofill is active (enabled and has data)
   */
  _computeStatus() {
    const savedFieldNames = Services.ppmm.sharedData.get(
      "FormAutofill:savedFieldNames"
    );

    return (
      (Services.prefs.getBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF) ||
        Services.prefs.getBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF)) &&
      savedFieldNames &&
      savedFieldNames.size > 0
    );
  },

  /**
   * Update the status and trigger _onStatusChanged, if necessary.
   */
  _updateStatus() {
    log.debug("_updateStatus");
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
  async receiveMessage({ name, data, target }) {
    switch (name) {
      case "FormAutofill:InitStorage": {
        await this.formAutofillStorage.initialize();
        break;
      }
      case "FormAutofill:GetRecords": {
        await this._getRecords(data, target);
        break;
      }
      case "FormAutofill:SaveAddress": {
        if (data.guid) {
          await this.formAutofillStorage.addresses.update(
            data.guid,
            data.address
          );
        } else {
          await this.formAutofillStorage.addresses.add(data.address);
        }
        break;
      }
      case "FormAutofill:SaveCreditCard": {
        if (!(await OSKeyStore.ensureLoggedIn())) {
          log.warn("User canceled encryption login");
          return;
        }
        await this.formAutofillStorage.creditCards.add(data.creditcard);
        break;
      }
      case "FormAutofill:RemoveAddresses": {
        data.guids.forEach(guid =>
          this.formAutofillStorage.addresses.remove(guid)
        );
        break;
      }
      case "FormAutofill:RemoveCreditCards": {
        data.guids.forEach(guid =>
          this.formAutofillStorage.creditCards.remove(guid)
        );
        break;
      }
      case "FormAutofill:OnFormSubmit": {
        await this._onFormSubmit(data, target);
        break;
      }
      case "FormAutofill:OpenPreferences": {
        const win = BrowserWindowTracker.getTopWindow();
        win.openPreferences("privacy-form-autofill");
        break;
      }
      case "FormAutofill:GetDecryptedString": {
        let { cipherText, reauth } = data;
        let string;
        try {
          string = await OSKeyStore.decrypt(cipherText, reauth);
        } catch (e) {
          if (e.result != Cr.NS_ERROR_ABORT) {
            throw e;
          }
          log.warn("User canceled encryption login");
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
    this.formAutofillStorage._saveImmediately();

    Services.ppmm.removeMessageListener("FormAutofill:InitStorage", this);
    Services.ppmm.removeMessageListener("FormAutofill:GetRecords", this);
    Services.ppmm.removeMessageListener("FormAutofill:SaveAddress", this);
    Services.ppmm.removeMessageListener("FormAutofill:RemoveAddresses", this);
    Services.obs.removeObserver(this, "privacy-pane-loaded");
    Services.prefs.removeObserver(ENABLED_AUTOFILL_ADDRESSES_PREF, this);
    Services.wm.removeListener(this);

    if (FormAutofill.isAutofillCreditCardsAvailable) {
      Services.ppmm.removeMessageListener("FormAutofill:SaveCreditCard", this);
      Services.ppmm.removeMessageListener(
        "FormAutofill:RemoveCreditCards",
        this
      );
      Services.ppmm.removeMessageListener(
        "FormAutofill:GetDecryptedString",
        this
      );
      Services.prefs.removeObserver(ENABLED_AUTOFILL_CREDITCARDS_PREF, this);
    }
  },

  /**
   * Get the records from profile store and return results back to content
   * process. It will decrypt the credit card number and append
   * "cc-number-decrypted" to each record if OSKeyStore isn't set.
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
  async _getRecords({ collectionName, searchString, info }, target) {
    let collection = this.formAutofillStorage[collectionName];
    if (!collection) {
      target.sendAsyncMessage("FormAutofill:Records", []);
      return;
    }

    let recordsInCollection = await collection.getAll();
    if (!info || !info.fieldName || !recordsInCollection.length) {
      target.sendAsyncMessage("FormAutofill:Records", recordsInCollection);
      return;
    }

    let isCC = collectionName == CREDITCARDS_COLLECTION_NAME;
    // We don't filter "cc-number"
    if (isCC && info.fieldName == "cc-number") {
      recordsInCollection = recordsInCollection.filter(
        record => !!record["cc-number"]
      );
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

      if (
        collectionName == ADDRESSES_COLLECTION_NAME &&
        record.country &&
        !FormAutofill.supportedCountries.includes(record.country)
      ) {
        // Address autofill isn't supported for the record's country so we don't
        // want to attempt to potentially incorrectly fill the address fields.
        continue;
      }

      if (
        lcSearchString &&
        !String(fieldValue)
          .toLowerCase()
          .startsWith(lcSearchString)
      ) {
        continue;
      }
      records.push(record);
    }

    target.sendAsyncMessage("FormAutofill:Records", records);
  },

  _updateSavedFieldNames() {
    log.debug("_updateSavedFieldNames");

    let savedFieldNames;
    // Don't access the credit cards store unless it is enabled.
    if (FormAutofill.isAutofillCreditCardsAvailable) {
      savedFieldNames = new Set([
        ...this.formAutofillStorage.addresses.getSavedFieldNames(),
        ...this.formAutofillStorage.creditCards.getSavedFieldNames(),
      ]);
    } else {
      savedFieldNames = this.formAutofillStorage.addresses.getSavedFieldNames();
    }

    Services.ppmm.sharedData.set(
      "FormAutofill:savedFieldNames",
      savedFieldNames
    );
    Services.ppmm.sharedData.flush();

    this._updateStatus();
  },

  async _onAddressSubmit(address, target, timeStartedFillingMS) {
    let showDoorhanger = null;
    if (address.guid) {
      // Avoid updating the fields that users don't modify.
      let originalAddress = await this.formAutofillStorage.addresses.get(
        address.guid
      );
      for (let field in address.record) {
        if (address.untouchedFields.includes(field) && originalAddress[field]) {
          address.record[field] = originalAddress[field];
        }
      }

      if (
        !(await this.formAutofillStorage.addresses.mergeIfPossible(
          address.guid,
          address.record,
          true
        ))
      ) {
        this._recordFormFillingTime(
          "address",
          "autofill-update",
          timeStartedFillingMS
        );

        showDoorhanger = async () => {
          const description = FormAutofillUtils.getAddressLabel(address.record);
          const state = await FormAutofillDoorhanger.show(
            target,
            "updateAddress",
            description
          );
          let changedGUIDs = await this.formAutofillStorage.addresses.mergeToStorage(
            address.record,
            true
          );
          switch (state) {
            case "create":
              if (!changedGUIDs.length) {
                changedGUIDs.push(
                  await this.formAutofillStorage.addresses.add(address.record)
                );
              }
              break;
            case "update":
              if (!changedGUIDs.length) {
                await this.formAutofillStorage.addresses.update(
                  address.guid,
                  address.record,
                  true
                );
                changedGUIDs.push(address.guid);
              } else {
                this.formAutofillStorage.addresses.remove(address.guid);
              }
              break;
          }
          changedGUIDs.forEach(guid =>
            this.formAutofillStorage.addresses.notifyUsed(guid)
          );
        };
        // Address should be updated
        Services.telemetry.scalarAdd(
          "formautofill.addresses.fill_type_autofill_update",
          1
        );
      } else {
        this._recordFormFillingTime(
          "address",
          "autofill",
          timeStartedFillingMS
        );
        this.formAutofillStorage.addresses.notifyUsed(address.guid);
        // Address is merged successfully
        Services.telemetry.scalarAdd(
          "formautofill.addresses.fill_type_autofill",
          1
        );
      }
    } else {
      let changedGUIDs = await this.formAutofillStorage.addresses.mergeToStorage(
        address.record
      );
      if (!changedGUIDs.length) {
        changedGUIDs.push(
          await this.formAutofillStorage.addresses.add(address.record)
        );
      }
      changedGUIDs.forEach(guid =>
        this.formAutofillStorage.addresses.notifyUsed(guid)
      );
      this._recordFormFillingTime("address", "manual", timeStartedFillingMS);

      // Show first time use doorhanger
      if (FormAutofill.isAutofillAddressesFirstTimeUse) {
        Services.prefs.setBoolPref(
          FormAutofill.ADDRESSES_FIRST_TIME_USE_PREF,
          false
        );
        showDoorhanger = async () => {
          const description = FormAutofillUtils.getAddressLabel(address.record);
          const state = await FormAutofillDoorhanger.show(
            target,
            "firstTimeUse",
            description
          );
          if (state !== "open-pref") {
            return;
          }

          target.ownerGlobal.openPreferences("privacy-address-autofill");
        };
      } else {
        // We want to exclude the first time form filling.
        Services.telemetry.scalarAdd(
          "formautofill.addresses.fill_type_manual",
          1
        );
      }
    }
    return showDoorhanger;
  },

  async _onCreditCardSubmit(creditCard, target, timeStartedFillingMS) {
    // Updates the used status for shield/heartbeat to recognize users who have
    // used Credit Card Autofill.
    let setUsedStatus = status => {
      if (FormAutofill.AutofillCreditCardsUsedStatus < status) {
        Services.prefs.setIntPref(
          FormAutofill.CREDITCARDS_USED_STATUS_PREF,
          status
        );
      }
    };

    // Remove invalid cc-type values
    if (
      creditCard.record["cc-type"] &&
      !CreditCard.isValidNetwork(creditCard.record["cc-type"])
    ) {
      delete creditCard.record["cc-type"];
    }

    // We'll show the credit card doorhanger if:
    //   - User applys autofill and changed
    //   - User fills form manually and the filling data is not duplicated to storage
    if (creditCard.guid) {
      // Indicate that the user has used Credit Card Autofill to fill in a form.
      setUsedStatus(3);

      let originalCCData = await this.formAutofillStorage.creditCards.get(
        creditCard.guid
      );
      let recordUnchanged = true;
      for (let field in creditCard.record) {
        if (creditCard.record[field] === "" && !originalCCData[field]) {
          continue;
        }
        // Avoid updating the fields that users don't modify, but skip number field
        // because we don't want to trigger decryption here.
        let untouched = creditCard.untouchedFields.includes(field);
        if (untouched && field !== "cc-number") {
          creditCard.record[field] = originalCCData[field];
        }
        // recordUnchanged will be false if one of the field is changed.
        recordUnchanged &= untouched;
      }

      if (recordUnchanged) {
        this.formAutofillStorage.creditCards.notifyUsed(creditCard.guid);
        // Add probe to record credit card autofill(without modification).
        Services.telemetry.scalarAdd(
          "formautofill.creditCards.fill_type_autofill",
          1
        );
        this._recordFormFillingTime(
          "creditCard",
          "autofill",
          timeStartedFillingMS
        );
        return false;
      }
      // Add the probe to record credit card autofill with modification.
      Services.telemetry.scalarAdd(
        "formautofill.creditCards.fill_type_autofill_modified",
        1
      );
      this._recordFormFillingTime(
        "creditCard",
        "autofill-update",
        timeStartedFillingMS
      );
    } else {
      // Indicate that the user neither sees the doorhanger nor uses Autofill
      // but somehow has a duplicate record in the storage. Will be reset to 2
      // if the doorhanger actually shows below.
      setUsedStatus(1);

      // Add the probe to record credit card manual filling.
      Services.telemetry.scalarAdd(
        "formautofill.creditCards.fill_type_manual",
        1
      );
      this._recordFormFillingTime("creditCard", "manual", timeStartedFillingMS);
    }

    // Early return if it's a duplicate data
    let dupGuid = await this.formAutofillStorage.creditCards.getDuplicateGuid(
      creditCard.record
    );
    if (dupGuid) {
      this.formAutofillStorage.creditCards.notifyUsed(dupGuid);
      return false;
    }

    // Indicate that the user has seen the doorhanger.
    setUsedStatus(2);

    return async () => {
      // Suppress the pending doorhanger from showing up if user disabled credit card in previous doorhanger.
      if (!FormAutofill.isAutofillCreditCardsEnabled) {
        return;
      }

      let number =
        creditCard.record["cc-number"] ||
        creditCard.record["cc-number-decrypted"];
      let name = creditCard.record["cc-name"];
      const description = await CreditCard.getLabel({ name, number });
      const state = await FormAutofillDoorhanger.show(
        target,
        creditCard.guid ? "updateCreditCard" : "addCreditCard",
        description
      );
      if (state == "cancel") {
        return;
      }

      if (state == "disable") {
        Services.prefs.setBoolPref(
          "extensions.formautofill.creditCards.enabled",
          false
        );
        return;
      }

      if (!(await OSKeyStore.ensureLoggedIn())) {
        log.warn("User canceled encryption login");
        return;
      }

      let changedGUIDs = [];
      if (creditCard.guid) {
        if (state == "update") {
          await this.formAutofillStorage.creditCards.update(
            creditCard.guid,
            creditCard.record,
            true
          );
          changedGUIDs.push(creditCard.guid);
        } else if ("create") {
          changedGUIDs.push(
            await this.formAutofillStorage.creditCards.add(creditCard.record)
          );
        }
      } else {
        changedGUIDs.push(
          ...(await this.formAutofillStorage.creditCards.mergeToStorage(
            creditCard.record
          ))
        );
        if (!changedGUIDs.length) {
          changedGUIDs.push(
            await this.formAutofillStorage.creditCards.add(creditCard.record)
          );
        }
      }
      changedGUIDs.forEach(guid =>
        this.formAutofillStorage.creditCards.notifyUsed(guid)
      );
    };
  },

  async _onFormSubmit(data, target) {
    let {
      profile: { address, creditCard },
      timeStartedFillingMS,
    } = data;

    // Don't record filling time if any type of records has more than one section being
    // populated. We've been recording the filling time, so the other cases that aren't
    // recorded on the same basis should be out of the data samples. E.g. Filling time of
    // populating one profile is different from populating two sections, therefore, we
    // shouldn't record the later to regress the representation of existing statistics.
    if (address.length > 1 || creditCard.length > 1) {
      timeStartedFillingMS = null;
    }

    // Transmit the telemetry immediately in the meantime form submitted, and handle these pending
    // doorhangers at a later.
    await Promise.all(
      [
        await Promise.all(
          address.map(addrRecord =>
            this._onAddressSubmit(addrRecord, target, timeStartedFillingMS)
          )
        ),
        await Promise.all(
          creditCard.map(ccRecord =>
            this._onCreditCardSubmit(ccRecord, target, timeStartedFillingMS)
          )
        ),
      ]
        .map(pendingDoorhangers => {
          return pendingDoorhangers.filter(
            pendingDoorhanger =>
              !!pendingDoorhanger && typeof pendingDoorhanger == "function"
          );
        })
        .map(pendingDoorhangers =>
          (async () => {
            for (const showDoorhanger of pendingDoorhangers) {
              await showDoorhanger();
            }
          })()
        )
    );
  },
  /**
   * Set the probes for the filling time with specific filling type and form type.
   *
   * @private
   * @param  {string} formType
   *         3 type of form (address/creditcard/address-creditcard).
   * @param  {string} fillingType
   *         3 filling type (manual/autofill/autofill-update).
   * @param  {int|null} startedFillingMS
   *         Time that form started to filling in ms. Early return if start time is null.
   */
  _recordFormFillingTime(formType, fillingType, startedFillingMS) {
    if (!startedFillingMS) {
      return;
    }
    let histogram = Services.telemetry.getKeyedHistogramById(
      "FORM_FILLING_REQUIRED_TIME_MS"
    );
    histogram.add(`${formType}-${fillingType}`, Date.now() - startedFillingMS);
  },
};

var formAutofillParent = new FormAutofillParent();
