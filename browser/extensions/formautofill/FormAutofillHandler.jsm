/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Defines a handler object to represent forms that autofill can handle.
 */

/* exported FormAutofillHandler */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillHandler"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillHeuristics",
                                  "resource://formautofill/FormAutofillHeuristics.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormLikeFactory",
                                  "resource://gre/modules/FormLikeFactory.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const {FIELD_STATES} = FormAutofillUtils;

class FormAutofillSection {
  constructor(fieldDetails, winUtils) {
    this.address = {
      /**
       * Similar to the `_validDetails` but contains address fields only.
       */
      fieldDetails: [],
      /**
       * String of the filled address' guid.
       */
      filledRecordGUID: null,
    };
    this.creditCard = {
      /**
       * Similar to the `_validDetails` but contains credit card fields only.
       */
      fieldDetails: [],
      /**
       * String of the filled creditCard's' guid.
       */
      filledRecordGUID: null,
    };

    /**
     * Enum for form autofill MANUALLY_MANAGED_STATES values
     */
    this._FIELD_STATE_ENUM = {
      // not themed
      [FIELD_STATES.NORMAL]: null,
      // highlighted
      [FIELD_STATES.AUTO_FILLED]: "-moz-autofill",
      // highlighted && grey color text
      [FIELD_STATES.PREVIEW]: "-moz-autofill-preview",
    };

    this.winUtils = winUtils;

    this.address.fieldDetails = fieldDetails.filter(
      detail => FormAutofillUtils.isAddressField(detail.fieldName)
    );
    if (this.address.fieldDetails.length < FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD) {
      log.debug("Ignoring address related fields since the section has only",
                this.address.fieldDetails.length,
                "field(s)");
      this.address.fieldDetails = [];
    }

    this.creditCard.fieldDetails = fieldDetails.filter(
      detail => FormAutofillUtils.isCreditCardField(detail.fieldName)
    );
    if (!this._isValidCreditCardForm(this.creditCard.fieldDetails)) {
      log.debug("Invalid credit card section.");
      this.creditCard.fieldDetails = [];
    }

    this._cacheValue = {
      allFieldNames: null,
      oneLineStreetAddress: null,
      matchingSelectOption: null,
    };

    this._validDetails = Array.of(...(this.address.fieldDetails),
                                  ...(this.creditCard.fieldDetails));
    log.debug(this._validDetails.length, "valid fields in the section is collected.");
  }

  get validDetails() {
    return this._validDetails;
  }

  getFieldDetailByElement(element) {
    return this._validDetails.find(
      detail => detail.elementWeakRef.get() == element
    );
  }

  _isValidCreditCardForm(fieldDetails) {
    let ccNumberReason = "";
    let hasCCNumber = false;
    let hasExpiryDate = false;

    for (let detail of fieldDetails) {
      switch (detail.fieldName) {
        case "cc-number":
          hasCCNumber = true;
          ccNumberReason = detail._reason;
          break;
        case "cc-exp":
        case "cc-exp-month":
        case "cc-exp-year":
          hasExpiryDate = true;
          break;
      }
    }

    return hasCCNumber && (ccNumberReason == "autocomplete" || hasExpiryDate);
  }

  get allFieldNames() {
    if (!this._cacheValue.allFieldNames) {
      this._cacheValue.allFieldNames = this._validDetails.map(record => record.fieldName);
    }
    return this._cacheValue.allFieldNames;
  }

  getFieldDetailByName(fieldName) {
    return this._validDetails.find(detail => detail.fieldName == fieldName);
  }

  _getTargetSet(element) {
    let fieldDetail = this.getFieldDetailByElement(element);
    if (!fieldDetail) {
      return null;
    }
    if (FormAutofillUtils.isAddressField(fieldDetail.fieldName)) {
      return this.address;
    }
    if (FormAutofillUtils.isCreditCardField(fieldDetail.fieldName)) {
      return this.creditCard;
    }
    return null;
  }

  getFieldDetailsByElement(element) {
    let targetSet = this._getTargetSet(element);
    return targetSet ? targetSet.fieldDetails : [];
  }

  getFilledRecordGUID(element) {
    let targetSet = this._getTargetSet(element);
    return targetSet ? targetSet.filledRecordGUID : null;
  }

  _getOneLineStreetAddress(address) {
    if (!this._cacheValue.oneLineStreetAddress) {
      this._cacheValue.oneLineStreetAddress = {};
    }
    if (!this._cacheValue.oneLineStreetAddress[address]) {
      this._cacheValue.oneLineStreetAddress[address] = FormAutofillUtils.toOneLineAddress(address);
    }
    return this._cacheValue.oneLineStreetAddress[address];
  }

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
  }

  /**
   * Replace tel with tel-national if tel violates the input element's
   * restriction.
   * @param {Object} profile
   *        A profile to be converted.
   */
  _telTransformer(profile) {
    if (!profile.tel || !profile["tel-national"]) {
      return;
    }

    let detail = this.getFieldDetailByName("tel");
    if (!detail) {
      return;
    }

    let element = detail.elementWeakRef.get();
    let _pattern;
    let testPattern = str => {
      if (!_pattern) {
        // The pattern has to match the entire value.
        _pattern = new RegExp("^(?:" + element.pattern + ")$", "u");
      }
      return _pattern.test(str);
    };
    if (element.pattern) {
      if (testPattern(profile.tel)) {
        return;
      }
    } else if (element.maxLength) {
      if (detail._reason == "autocomplete" && profile.tel.length <= element.maxLength) {
        return;
      }
    }

    if (detail._reason != "autocomplete") {
      // Since we only target people living in US and using en-US websites in
      // MVP, it makes more sense to fill `tel-national` instead of `tel`
      // if the field is identified by heuristics and no other clues to
      // determine which one is better.
      // TODO: [Bug 1407545] This should be improved once more countries are
      // supported.
      profile.tel = profile["tel-national"];
    } else if (element.pattern) {
      if (testPattern(profile["tel-national"])) {
        profile.tel = profile["tel-national"];
      }
    } else if (element.maxLength) {
      if (profile["tel-national"].length <= element.maxLength) {
        profile.tel = profile["tel-national"];
      }
    }
  }

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
      if (ChromeUtils.getClassName(element) !== "HTMLSelectElement") {
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
  }

  _creditCardExpDateTransformer(profile) {
    if (!profile["cc-exp"]) {
      return;
    }

    let detail = this.getFieldDetailByName("cc-exp");
    if (!detail) {
      return;
    }

    let element = detail.elementWeakRef.get();
    if (element.tagName != "INPUT" || !element.placeholder) {
      return;
    }

    let result,
      ccExpMonth = profile["cc-exp-month"],
      ccExpYear = profile["cc-exp-year"],
      placeholder = element.placeholder;

    result = /(?:[^m]|\b)(m{1,2})\s*([-/\\]*)\s*(y{2,4})(?!y)/i.exec(placeholder);
    if (result) {
      profile["cc-exp"] = String(ccExpMonth).padStart(result[1].length, "0") +
                          result[2] +
                          String(ccExpYear).substr(-1 * result[3].length);
      return;
    }

    result = /(?:[^y]|\b)(y{2,4})\s*([-/\\]*)\s*(m{1,2})(?!m)/i.exec(placeholder);
    if (result) {
      profile["cc-exp"] = String(ccExpYear).substr(-1 * result[1].length) +
                          result[2] +
                          String(ccExpMonth).padStart(result[3].length, "0");
    }
  }

  _adaptFieldMaxLength(profile) {
    for (let key in profile) {
      let detail = this.getFieldDetailByName(key);
      if (!detail) {
        continue;
      }

      let element = detail.elementWeakRef.get();
      if (!element) {
        continue;
      }

      let maxLength = element.maxLength;
      if (maxLength === undefined || maxLength < 0 || profile[key].length <= maxLength) {
        continue;
      }

      if (maxLength) {
        profile[key] = profile[key].substr(0, maxLength);
      } else {
        delete profile[key];
      }
    }
  }

  getAdaptedProfiles(originalProfiles) {
    for (let profile of originalProfiles) {
      this._addressTransformer(profile);
      this._telTransformer(profile);
      this._matchSelectOptions(profile);
      this._creditCardExpDateTransformer(profile);
      this._adaptFieldMaxLength(profile);
    }
    return originalProfiles;
  }

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   * @param {HTMLElement} focusedInput
   *        A focused input element needed to determine the address or credit
   *        card field.
   */
  async autofillFields(profile, focusedInput) {
    let focusedDetail = this.getFieldDetailByElement(focusedInput);
    if (!focusedDetail) {
      throw new Error("No fieldDetail for the focused input.");
    }
    let targetSet = this._getTargetSet(focusedInput);
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
    }

    log.debug("profile in autofillFields:", profile);

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
          this.changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
        }
      } else if (ChromeUtils.getClassName(element) === "HTMLSelectElement") {
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
        this.changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
      }
      if (fieldDetail.state == FIELD_STATES.AUTO_FILLED) {
        element.addEventListener("input", this, {mozSystemGroup: true});
      }
    }
  }

  /**
   * Populates result to the preview layers with given profile.
   *
   * @param {Object} profile
   *        A profile to be previewed with
   * @param {HTMLElement} focusedInput
   *        A focused input element for determining credit card or address fields.
   */
  previewFormFields(profile, focusedInput) {
    log.debug("preview profile: ", profile);

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

      if (ChromeUtils.getClassName(element) === "HTMLSelectElement") {
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
      this.changeFieldState(fieldDetail, value ? FIELD_STATES.PREVIEW : FIELD_STATES.NORMAL);
    }
  }

  /**
   * Clear preview text and background highlight of all fields.
   *
   * @param {HTMLElement} focusedInput
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
      if (fieldDetail.state == FIELD_STATES.AUTO_FILLED) {
        continue;
      }

      this.changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
  }

  /**
   * Clear value and highlight style of all filled fields.
   *
   * @param {Object} focusedInput
   *        A focused input element for determining credit card or address fields.
   */
  clearPopulatedForm(focusedInput) {
    let fieldDetails = this.getFieldDetailsByElement(focusedInput);
    for (let fieldDetail of fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      if (!element) {
        log.warn(fieldDetail.fieldName, "is unreachable");
        continue;
      }

      // Only reset value for input element.
      if (fieldDetail.state == FIELD_STATES.AUTO_FILLED &&
          element instanceof Ci.nsIDOMHTMLInputElement) {
        element.setUserInput("");
      }
    }
    this.resetFieldStates();
  }

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
    if (!(nextState in this._FIELD_STATE_ENUM)) {
      log.warn(fieldDetail.fieldName, "is trying to change to an invalid state");
      return;
    }

    for (let [state, mmStateValue] of Object.entries(this._FIELD_STATE_ENUM)) {
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
  }

  resetFieldStates() {
    for (let fieldDetail of this._validDetails) {
      const element = fieldDetail.elementWeakRef.get();
      element.removeEventListener("input", this, {mozSystemGroup: true});
      this.changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
    this.address.filledRecordGUID = null;
    this.creditCard.filledRecordGUID = null;
  }

  isFilled() {
    return !!(this.address.filledRecordGUID || this.creditCard.filledRecordGUID);
  }

  _isAddressRecordCreatable(record) {
    let hasName = 0;
    let length = 0;
    for (let key of Object.keys(record)) {
      if (!record[key]) {
        continue;
      }
      if (FormAutofillUtils.getCategoryFromFieldName(key) == "name") {
        hasName = 1;
        continue;
      }
      length++;
    }
    return (length + hasName) >= FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD;
  }

  _isCreditCardRecordCreatable(record) {
    return record["cc-number"] && FormAutofillUtils.isCCNumber(record["cc-number"]);
  }

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
    let target = [];

    if (FormAutofillUtils.isAutofillAddressesEnabled) {
      target.push("address");
    }
    if (FormAutofillUtils.isAutofillCreditCardsEnabled) {
      target.push("creditCard");
    }

    target.forEach(type => {
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
            ChromeUtils.getClassName(element) === "HTMLSelectElement") {
          // Don't save the record when the option value is empty *OR* there
          // are multiple options being selected. The empty option is usually
          // assumed to be default along with a meaningless text to users.
          if (!value || element.selectedOptions.length != 1) {
            // Keep the property and preserve more information for address updating
            data[type].record[detail.fieldName] = "";
            return;
          }

          let text = element.selectedOptions[0].text.trim();
          value = FormAutofillUtils.getAbbreviatedSubregionName([value, text]) || text;
        }

        if (!value || value.length > FormAutofillUtils.MAX_FIELD_VALUE_LENGTH) {
          // Keep the property and preserve more information for updating
          data[type].record[detail.fieldName] = "";
          return;
        }

        data[type].record[detail.fieldName] = value;

        if (detail.state == FIELD_STATES.AUTO_FILLED) {
          data[type].untouchedFields.push(detail.fieldName);
        }
      });
    });

    this._normalizeAddress(data.address);

    if (data.address && !this._isAddressRecordCreatable(data.address.record)) {
      log.debug("No address record saving since there are only",
                Object.keys(data.address.record).length,
                "usable fields");
      delete data.address;
    }

    if (data.creditCard && !this._isCreditCardRecordCreatable(data.creditCard.record)) {
      log.debug("No credit card record saving since card number is invalid");
      delete data.creditCard;
    }

    // If both address and credit card exists, skip this metrics because it not a
    // general case and each specific histogram might contains insufficient data set.
    if (data.address && data.creditCard) {
      this.timeStartedFillingMS = null;
    }

    return data;
  }

  _normalizeAddress(address) {
    if (!address) {
      return;
    }

    // Normalize Country
    if (address.record.country) {
      let detail = this.getFieldDetailByName("country");
      // Try identifying country field aggressively if it doesn't come from
      // @autocomplete.
      if (detail._reason != "autocomplete") {
        let countryCode = FormAutofillUtils.identifyCountryCode(address.record.country);
        if (countryCode) {
          address.record.country = countryCode;
        }
      }
    }

    // Normalize Tel
    FormAutofillUtils.compressTel(address.record);
    if (address.record.tel) {
      let allTelComponentsAreUntouched = Object.keys(address.record)
        .filter(field => FormAutofillUtils.getCategoryFromFieldName(field) == "tel")
        .every(field => address.untouchedFields.includes(field));
      if (allTelComponentsAreUntouched) {
        // No need to verify it if none of related fields are modified after autofilling.
        if (!address.untouchedFields.includes("tel")) {
          address.untouchedFields.push("tel");
        }
      } else {
        let strippedNumber = address.record.tel.replace(/[\s\(\)-]/g, "");

        // Remove "tel" if it contains invalid characters or the length of its
        // number part isn't between 5 and 15.
        // (The maximum length of a valid number in E.164 format is 15 digits
        //  according to https://en.wikipedia.org/wiki/E.164 )
        if (!/^(\+?)[\da-zA-Z]{5,15}$/.test(strippedNumber)) {
          address.record.tel = "";
        }
      }
    }
  }

  async _decrypt(cipherText, reauth) {
    return new Promise((resolve) => {
      Services.cpmm.addMessageListener("FormAutofill:DecryptedString", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:DecryptedString", getResult);
        resolve(result.data);
      });

      Services.cpmm.sendAsyncMessage("FormAutofill:GetDecryptedString", {cipherText, reauth});
    });
  }

  handleEvent(event) {
    switch (event.type) {
      case "input": {
        if (!event.isTrusted) {
          return;
        }
        const target = event.target;
        const fieldDetail = this.getFieldDetailByElement(target);
        const targetSet = this._getTargetSet(target);
        this.changeFieldState(fieldDetail, FIELD_STATES.NORMAL);

        if (!targetSet.fieldDetails.some(detail => detail.state == FIELD_STATES.AUTO_FILLED)) {
          targetSet.filledRecordGUID = null;
        }
        target.removeEventListener("input", this, {mozSystemGroup: true});
        break;
      }
    }
  }
}

/**
 * Handles profile autofill for a DOM Form element.
 */
class FormAutofillHandler {
  /**
   * Initialize the form from `FormLike` object to handle the section or form
   * operations.
   * @param {FormLike} form Form that need to be auto filled
   */
  constructor(form) {
    this._updateForm(form);

    /**
     * A WindowUtils reference of which Window the form belongs
     */
    this.winUtils = this.form.rootElement.ownerGlobal.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);

    /**
     * Time in milliseconds since epoch when a user started filling in the form.
     */
    this.timeStartedFillingMS = null;
  }

  /**
   * Check the form is necessary to be updated. This function should be able to
   * detect any changes including all control elements in the form.
   * @param {HTMLElement} element The element supposed to be in the form.
   * @returns {boolean} FormAutofillHandler.form is updated or not.
   */
  updateFormIfNeeded(element) {
    // When the following condition happens, FormAutofillHandler.form should be
    // updated:
    // * The count of form controls is changed.
    // * When the element can not be found in the current form.
    //
    // However, we should improve the function to detect the element changes.
    // e.g. a tel field is changed from type="hidden" to type="tel".

    let _formLike;
    let getFormLike = () => {
      if (!_formLike) {
        _formLike = FormLikeFactory.createFromField(element);
      }
      return _formLike;
    };

    let currentForm = element.form;
    if (!currentForm) {
      currentForm = getFormLike();
    }

    if (currentForm.elements.length != this.form.elements.length) {
      log.debug("The count of form elements is changed.");
      this._updateForm(getFormLike());
      return true;
    }

    if (this.form.elements.indexOf(element) === -1) {
      log.debug("The element can not be found in the current form.");
      this._updateForm(getFormLike());
      return true;
    }

    return false;
  }

  /**
   * Update the form with a new FormLike, and the related fields should be
   * updated or clear to ensure the data consistency.
   * @param {FormLike} form a new FormLike to replace the original one.
   */
  _updateForm(form) {
    /**
     * DOM Form element to which this object is attached.
     */
    this.form = form;

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
    this.fieldDetails = null;

    this.sections = [];
    this._sectionCache = new WeakMap();
  }

  /**
   * Set fieldDetails from the form about fields that can be autofilled.
   *
   * @param {boolean} allowDuplicates
   *        true to remain any duplicated field details otherwise to remove the
   *        duplicated ones.
   * @returns {Array} The valid address and credit card details.
   */
  collectFormFields(allowDuplicates = false) {
    let sections = FormAutofillHeuristics.getFormInfo(this.form, allowDuplicates);
    let allValidDetails = [];
    for (let fieldDetails of sections) {
      let section = new FormAutofillSection(fieldDetails, this.winUtils);
      this.sections.push(section);
      allValidDetails.push(...section.validDetails);
    }

    for (let detail of allValidDetails) {
      let input = detail.elementWeakRef.get();
      if (!input) {
        continue;
      }
      input.addEventListener("input", this, {mozSystemGroup: true});
    }

    this.fieldDetails = allValidDetails;
    return allValidDetails;
  }

  getSectionByElement(element) {
    let section = this._sectionCache.get(element);
    if (!section) {
      section = this.sections.find(
        s => s.getFieldDetailByElement(element)
      );
      this._sectionCache.set(element, section);
    }
    return section;
  }

  getAllFieldNames(focusedInput) {
    let section = this.getSectionByElement(focusedInput);
    return section.allFieldNames;
  }

  previewFormFields(profile, focusedInput) {
    let section = this.getSectionByElement(focusedInput);
    section.previewFormFields(profile, focusedInput);
  }

  clearPreviewedFormFields(focusedInput) {
    let section = this.getSectionByElement(focusedInput);
    section.clearPreviewedFormFields(focusedInput);
  }

  clearPopulatedForm(focusedInput) {
    let section = this.getSectionByElement(focusedInput);
    section.clearPopulatedForm(focusedInput);
  }

  getFilledRecordGUID(focusedInput) {
    let section = this.getSectionByElement(focusedInput);
    return section.getFilledRecordGUID(focusedInput);
  }

  getAdaptedProfiles(originalProfiles, focusedInput) {
    let section = this.getSectionByElement(focusedInput);
    section.getAdaptedProfiles(originalProfiles);
    return originalProfiles;
  }

  hasFilledSection() {
    return this.sections.some(section => section.isFilled());
  }

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   * @param {HTMLElement} focusedInput
   *        A focused input element needed to determine the address or credit
   *        card field.
   */
  async autofillFormFields(profile, focusedInput) {
    let noFilledSectionsPreviously = !this.hasFilledSection();
    await this.getSectionByElement(focusedInput).autofillFields(profile, focusedInput);

    const onChangeHandler = e => {
      if (!e.isTrusted) {
        return;
      }
      if (e.type == "reset") {
        for (let section of this.sections) {
          section.resetFieldStates();
        }
      }
      // Unregister listeners once no field is in AUTO_FILLED state.
      if (!this.hasFilledSection()) {
        this.form.rootElement.removeEventListener("input", onChangeHandler, {mozSystemGroup: true});
        this.form.rootElement.removeEventListener("reset", onChangeHandler, {mozSystemGroup: true});
      }
    };

    if (noFilledSectionsPreviously) {
      // Handle the highlight style resetting caused by user's correction afterward.
      log.debug("register change handler for filled form:", this.form);
      this.form.rootElement.addEventListener("input", onChangeHandler, {mozSystemGroup: true});
      this.form.rootElement.addEventListener("reset", onChangeHandler, {mozSystemGroup: true});
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "input":
        if (!event.isTrusted) {
          return;
        }

        for (let detail of this.fieldDetails) {
          let input = detail.elementWeakRef.get();
          if (!input) {
            continue;
          }
          input.removeEventListener("input", this, {mozSystemGroup: true});
        }
        this.timeStartedFillingMS = Date.now();
        break;
    }
  }

  /**
   * Collect the filled sections within submitted form and convert all the valid
   * field data into multiple records.
   *
   * @returns {Object} records
   *          {Array.<Object>} records.address
   *          {Array.<Object>} records.creditCard
   */
  createRecords() {
    const records = {
      address: [],
      creditCard: [],
    };

    for (const section of this.sections) {
      const secRecords = section.createRecords();
      for (const [type, record] of Object.entries(secRecords)) {
        records[type].push(record);
      }
    }
    log.debug("Create records:", records);
    return records;
  }
}
