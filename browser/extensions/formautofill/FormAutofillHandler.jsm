/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Defines a handler object to represent forms that autofill can handle.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillHandler"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillHeuristics",
                                  "resource://formautofill/FormAutofillHeuristics.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

/**
 * Handles profile autofill for a DOM Form element.
 * @param {FormLike} form Form that need to be auto filled
 */
function FormAutofillHandler(form) {
  this.form = form;
  this.fieldDetails = [];
  this.winUtils = this.form.rootElement.ownerGlobal.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils);

  this.address = {
    /**
     * Similar to the `fieldDetails` above but contains address fields only.
     */
    fieldDetails: [],
    /**
     * String of the filled address' guid.
     */
    filledRecordGUID: null,
  };

  this.creditCard = {
    /**
     * Similar to the `fieldDetails` above but contains credit card fields only.
     */
    fieldDetails: [],
    /**
     * String of the filled creditCard's guid.
     */
    filledRecordGUID: null,
  };
}

FormAutofillHandler.prototype = {
  /**
   * DOM Form element to which this object is attached.
   */
  form: null,

  _formFieldCount: 0,

  /**
   * Array of collected data about relevant form fields.  Each item is an object
   * storing the identifying details of the field and a reference to the
   * originally associated element from the form.
   *
   * The "section", "addressType", "contactType", and "fieldName" values are
   * used to identify the exact field when the serializable data is received
   * from the backend.  There cannot be multiple fields which have
   * the same exact combination of these values.
   *
   * A direct reference to the associated element cannot be sent to the user
   * interface because processing may be done in the parent process.
   */
  fieldDetails: null,

  /**
   * Subcategory of handler that contains address related data.
   */
  address: null,

  /**
   * Subcategory of handler that contains credit card related data.
   */
  creditCard: null,

  /**
   * A WindowUtils reference of which Window the form belongs
   */
  winUtils: null,

  /**
   * Enum for form autofill MANUALLY_MANAGED_STATES values
   */
  fieldStateEnum: {
    // not themed
    NORMAL: null,
    // highlighted
    AUTO_FILLED: "-moz-autofill",
    // highlighted && grey color text
    PREVIEW: "-moz-autofill-preview",
  },

  get isFormChangedSinceLastCollection() {
    // When the number of form controls is the same with last collection, it
    // can be recognized as there is no element changed. However, we should
    // improve the function to detect the element changes. e.g. a tel field
    // is changed from type="hidden" to type="tel".
    return this._formFieldCount != this.form.elements.length;
  },

  /**
   * Time in milliseconds since epoch when a user started filling in the form.
   */
  timeStartedFillingMS: null,

  /**
   * Set fieldDetails from the form about fields that can be autofilled.
   *
   * @param {boolean} allowDuplicates
   *        true to remain any duplicated field details otherwise to remove the
   *        duplicated ones.
   * @returns {Array} The valid address and credit card details.
   */
  collectFormFields(allowDuplicates = false) {
    this._cacheValue.allFieldNames = null;
    this._formFieldCount = this.form.elements.length;
    let fieldDetails = FormAutofillHeuristics.getFormInfo(this.form, allowDuplicates);
    this.fieldDetails = fieldDetails ? fieldDetails : [];
    this.form.rootElement.addEventListener("input", this);
    log.debug("Collected details on", this.fieldDetails.length, "fields");

    this.address.fieldDetails = this.fieldDetails.filter(
      detail => FormAutofillUtils.isAddressField(detail.fieldName)
    );
    this.creditCard.fieldDetails = this.fieldDetails.filter(
      detail => FormAutofillUtils.isCreditCardField(detail.fieldName)
    );

    if (this.address.fieldDetails.length < FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD) {
      log.debug("Ignoring address related fields since it has only",
                this.address.fieldDetails.length,
                "field(s)");
      this.address.fieldDetails = [];
    }

    if (!this.creditCard.fieldDetails.some(i => i.fieldName == "cc-number")) {
      log.debug("Ignoring credit card related fields since it's without credit card number field");
      this.creditCard.fieldDetails = [];
    }

    return Array.of(...(this.address.fieldDetails),
                    ...(this.creditCard.fieldDetails));
  },

  getFieldDetailByName(fieldName) {
    return this.fieldDetails.find(detail => detail.fieldName == fieldName);
  },

  getFieldDetailsByElement(element) {
    let fieldDetail = this.fieldDetails.find(
      detail => detail.elementWeakRef.get() == element
    );
    if (FormAutofillUtils.isAddressField(fieldDetail.fieldName)) {
      return this.address.fieldDetails;
    }
    if (FormAutofillUtils.isCreditCardField(fieldDetail.fieldName)) {
      return this.creditCard.fieldDetails;
    }
    return [];
  },

  _cacheValue: {
    allFieldNames: null,
    oneLineStreetAddress: null,
    matchingSelectOption: null,
  },

  get allFieldNames() {
    if (!this._cacheValue.allFieldNames) {
      this._cacheValue.allFieldNames = this.fieldDetails.map(record => record.fieldName);
    }
    return this._cacheValue.allFieldNames;
  },

  _getOneLineStreetAddress(address) {
    if (!this._cacheValue.oneLineStreetAddress) {
      this._cacheValue.oneLineStreetAddress = {};
    }
    if (!this._cacheValue.oneLineStreetAddress[address]) {
      this._cacheValue.oneLineStreetAddress[address] = FormAutofillUtils.toOneLineAddress(address);
    }
    return this._cacheValue.oneLineStreetAddress[address];
  },

  _addressTransformer(profile) {
    if (profile["street-address"]) {
      // "-moz-street-address-one-line" is used by the labels in
      // ProfileAutoCompleteResult.
      profile["-moz-street-address-one-line"] = this._getOneLineStreetAddress(profile["street-address"]);
      let streetAddressDetail = this.getFieldDetailByName("street-address");
      if (streetAddressDetail &&
          (streetAddressDetail.elementWeakRef.get() instanceof Ci.nsIDOMHTMLInputElement)) {
        profile["street-address"] = profile["-moz-street-address-one-line"];
      }

      let waitForConcat = [];
      for (let f of ["address-line3", "address-line2", "address-line1"]) {
        waitForConcat.unshift(profile[f]);
        if (this.getFieldDetailByName(f)) {
          if (waitForConcat.length > 1) {
            profile[f] = FormAutofillUtils.toOneLineAddress(waitForConcat);
          }
          waitForConcat = [];
        }
      }
    }
  },

  _matchSelectOptions(profile) {
    if (!this._cacheValue.matchingSelectOption) {
      this._cacheValue.matchingSelectOption = new WeakMap();
    }

    for (let fieldName in profile) {
      let fieldDetail = this.getFieldDetailByName(fieldName);
      if (!fieldDetail) {
        continue;
      }

      let element = fieldDetail.elementWeakRef.get();
      if (!(element instanceof Ci.nsIDOMHTMLSelectElement)) {
        continue;
      }

      let cache = this._cacheValue.matchingSelectOption.get(element) || {};
      let value = profile[fieldName];
      if (cache[value] && cache[value].get()) {
        continue;
      }

      let option = FormAutofillUtils.findSelectOption(element, profile, fieldName);
      if (option) {
        cache[value] = Cu.getWeakReference(option);
        this._cacheValue.matchingSelectOption.set(element, cache);
      } else {
        if (cache[value]) {
          delete cache[value];
          this._cacheValue.matchingSelectOption.set(element, cache);
        }
        // Delete the field so the phishing hint won't treat it as a "also fill"
        // field.
        delete profile[fieldName];
      }
    }
  },

  getAdaptedProfiles(originalProfiles) {
    for (let profile of originalProfiles) {
      this._addressTransformer(profile);
      this._matchSelectOptions(profile);
    }
    return originalProfiles;
  },

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   * @param {Object} focusedInput
   *        A focused input element needed to determine the address or credit
   *        card field.
   */
  async autofillFormFields(profile, focusedInput) {
    let focusedDetail = this.fieldDetails.find(
      detail => detail.elementWeakRef.get() == focusedInput
    );
    let targetSet;
    if (FormAutofillUtils.isCreditCardField(focusedDetail.fieldName)) {
      // When Master Password is enabled by users, the decryption process
      // should prompt Master Password dialog to get the decrypted credit
      // card number. Otherwise, the number can be decrypted with the default
      // password.
      if (profile["cc-number-encrypted"]) {
        let decrypted = await this._decrypt(profile["cc-number-encrypted"], true);

        if (!decrypted) {
          // Early return if the decrypted is empty or undefined
          return;
        }

        profile["cc-number"] = decrypted;
      }
      targetSet = this.creditCard;
    } else if (FormAutofillUtils.isAddressField(focusedDetail.fieldName)) {
      targetSet = this.address;
    } else {
      throw new Error("Unknown form fields");
    }

    log.debug("profile in autofillFormFields:", profile);

    targetSet.filledRecordGUID = profile.guid;
    for (let fieldDetail of targetSet.fieldDetails) {
      // Avoid filling field value in the following cases:
      // 1. a non-empty input field for an unfocused input
      // 2. the invalid value set
      // 3. value already chosen in select element

      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        continue;
      }

      element.previewValue = "";
      let value = profile[fieldDetail.fieldName];

      if (element instanceof Ci.nsIDOMHTMLInputElement && value) {
        // For the focused input element, it will be filled with a valid value
        // anyway.
        // For the others, the fields should be only filled when their values
        // are empty.
        if (element == focusedInput ||
            (element != focusedInput && !element.value)) {
          element.setUserInput(value);
          this.changeFieldState(fieldDetail, "AUTO_FILLED");
          continue;
        }
      }

      if (element instanceof Ci.nsIDOMHTMLSelectElement) {
        let cache = this._cacheValue.matchingSelectOption.get(element) || {};
        let option = cache[value] && cache[value].get();
        if (!option) {
          continue;
        }
        // Do not change value or dispatch events if the option is already selected.
        // Use case for multiple select is not considered here.
        if (!option.selected) {
          option.selected = true;
          element.dispatchEvent(new element.ownerGlobal.UIEvent("input", {bubbles: true}));
          element.dispatchEvent(new element.ownerGlobal.Event("change", {bubbles: true}));
        }
        // Autofill highlight appears regardless if value is changed or not
        this.changeFieldState(fieldDetail, "AUTO_FILLED");
      }
    }

    // Handle the highlight style resetting caused by user's correction afterward.
    log.debug("register change handler for filled form:", this.form);
    const onChangeHandler = e => {
      let hasFilledFields;

      if (!e.isTrusted) {
        return;
      }

      for (let fieldDetail of targetSet.fieldDetails) {
        let element = fieldDetail.elementWeakRef.get();

        if (!element) {
          return;
        }

        if (e.target == element || (e.target == element.form && e.type == "reset")) {
          this.changeFieldState(fieldDetail, "NORMAL");
        }

        hasFilledFields |= (fieldDetail.state == "AUTO_FILLED");
      }

      // Unregister listeners and clear guid once no field is in AUTO_FILLED state.
      if (!hasFilledFields) {
        this.form.rootElement.removeEventListener("input", onChangeHandler);
        this.form.rootElement.removeEventListener("reset", onChangeHandler);
        targetSet.filledRecordGUID = null;
      }
    };

    this.form.rootElement.addEventListener("input", onChangeHandler);
    this.form.rootElement.addEventListener("reset", onChangeHandler);
  },

  /**
   * Populates result to the preview layers with given profile.
   *
   * @param {Object} profile
   *        A profile to be previewed with
   * @param {Object} focusedInput
   *        A focused input element for determining credit card or address fields.
   */
  previewFormFields(profile, focusedInput) {
    log.debug("preview profile in autofillFormFields:", profile);

    // Always show the decrypted credit card number when Master Password is
    // disabled.
    if (profile["cc-number-decrypted"]) {
      profile["cc-number"] = profile["cc-number-decrypted"];
    }

    let fieldDetails = this.getFieldDetailsByElement(focusedInput);
    for (let fieldDetail of fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      let value = profile[fieldDetail.fieldName] || "";

      // Skip the field that is null
      if (!element) {
        continue;
      }

      if (element instanceof Ci.nsIDOMHTMLSelectElement) {
        // Unlike text input, select element is always previewed even if
        // the option is already selected.
        if (value) {
          let cache = this._cacheValue.matchingSelectOption.get(element) || {};
          let option = cache[value] && cache[value].get();
          if (option) {
            value = option.text || "";
          } else {
            value = "";
          }
        }
      } else if (element.value) {
        // Skip the field if it already has text entered.
        continue;
      }
      element.previewValue = value;
      this.changeFieldState(fieldDetail, value ? "PREVIEW" : "NORMAL");
    }
  },

  /**
   * Clear preview text and background highlight of all fields.
   *
   * @param {Object} focusedInput
   *        A focused input element for determining credit card or address fields.
   */
  clearPreviewedFormFields(focusedInput) {
    log.debug("clear previewed fields in:", this.form);

    let fieldDetails = this.getFieldDetailsByElement(focusedInput);
    for (let fieldDetail of fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        log.warn(fieldDetail.fieldName, "is unreachable");
        continue;
      }

      element.previewValue = "";

      // We keep the state if this field has
      // already been auto-filled.
      if (fieldDetail.state === "AUTO_FILLED") {
        continue;
      }

      this.changeFieldState(fieldDetail, "NORMAL");
    }
  },

  /**
   * Change the state of a field to correspond with different presentations.
   *
   * @param {Object} fieldDetail
   *        A fieldDetail of which its element is about to update the state.
   * @param {string} nextState
   *        Used to determine the next state
   */
  changeFieldState(fieldDetail, nextState) {
    let element = fieldDetail.elementWeakRef.get();

    if (!element) {
      log.warn(fieldDetail.fieldName, "is unreachable while changing state");
      return;
    }
    if (!(nextState in this.fieldStateEnum)) {
      log.warn(fieldDetail.fieldName, "is trying to change to an invalid state");
      return;
    }

    for (let [state, mmStateValue] of Object.entries(this.fieldStateEnum)) {
      // The NORMAL state is simply the absence of other manually
      // managed states so we never need to add or remove it.
      if (!mmStateValue) {
        continue;
      }

      if (state == nextState) {
        this.winUtils.addManuallyManagedState(element, mmStateValue);
      } else {
        this.winUtils.removeManuallyManagedState(element, mmStateValue);
      }
    }

    fieldDetail.state = nextState;
  },

  /**
   * Return the records that is converted from address/creditCard fieldDetails and
   * only valid form records are included.
   *
   * @returns {Object}
   *          Consists of two record objects: address, creditCard. Each one can
   *          be omitted if there's no valid fields. A record object consists of
   *          three properties:
   *            - guid: The id of the previously-filled profile or null if omitted.
   *            - record: A valid record converted from details with trimmed result.
   *            - untouchedFields: Fields that aren't touched after autofilling.
   */
  createRecords() {
    let data = {};

    ["address", "creditCard"].forEach(type => {
      let details = this[type].fieldDetails;
      if (!details || details.length == 0) {
        return;
      }

      data[type] = {
        guid: this[type].filledRecordGUID,
        record: {},
        untouchedFields: [],
      };

      details.forEach(detail => {
        let element = detail.elementWeakRef.get();
        // Remove the unnecessary spaces
        let value = element && element.value.trim();

        // Try to abbreviate the value of select element.
        if (type == "address" &&
            detail.fieldName == "address-level1" &&
            element instanceof Ci.nsIDOMHTMLSelectElement) {
          // Don't save the record when the option value is empty *OR* there
          // are multiple options being selected. The empty option is usually
          // assumed to be default along with a meaningless text to users.
          if (!value || element.selectedOptions.length != 1) {
            return;
          }

          let text = element.selectedOptions[0].text.trim();
          value = FormAutofillUtils.getAbbreviatedStateName([value, text]) || text;
        }

        if (!value) {
          return;
        }

        data[type].record[detail.fieldName] = value;

        if (detail.state == "AUTO_FILLED") {
          data[type].untouchedFields.push(detail.fieldName);
        }
      });
    });

    if (data.address &&
        Object.keys(data.address.record).length < FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD) {
      log.debug("No address record saving since there are only",
                     Object.keys(data.address.record).length,
                     "usable fields");
      delete data.address;
    }

    if (data.creditCard && (!data.creditCard.record["cc-number"] ||
        !FormAutofillUtils.isCCNumber(data.creditCard.record["cc-number"]))) {
      log.debug("No credit card record saving since card number is invalid");
      delete data.creditCard;
    }

    return data;
  },

  async _decrypt(cipherText, reauth) {
    return new Promise((resolve) => {
      Services.cpmm.addMessageListener("FormAutofill:DecryptedString", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:DecryptedString", getResult);
        resolve(result.data);
      });

      Services.cpmm.sendAsyncMessage("FormAutofill:GetDecryptedString", {cipherText, reauth});
    });
  },

  /**
   * Find the fieldDetail by HTML element (assume all details were collected in collectFormFields).
   *
   * @param {HTMLElement} element
   *
   * @returns {Object|null}
   *          Return fieldDetail if fieldDetail's element ref could match the target.
   *          (or return null if the element could not match any fieldDetail).
   */
  getFieldDetailsForElement(element) {
    for (let detail of this.fieldDetails) {
      if (detail.elementWeakRef.get() == element) {
        return detail;
      }
    }
    return null;
  },

  handleEvent(event) {
    switch (event.type) {
      case "input":
        if (!event.isTrusted) {
          return;
        }

        if (!FormAutofillUtils.isFieldEligibleForAutofill(event.target)) {
          return;
        }

        if (!this.getFieldDetailsForElement(event.target)) {
          return;
        }

        this.form.rootElement.removeEventListener("input", this);
        this.timeStartedFillingMS = Date.now();
        break;
    }
  },
};
