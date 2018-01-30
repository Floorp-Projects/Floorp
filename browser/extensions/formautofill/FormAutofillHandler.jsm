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
    this.fieldDetails = fieldDetails;
    this.filledRecordGUID = null;
    this.winUtils = winUtils;

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

    if (!this.isValidSection()) {
      this.fieldDetails = [];
      log.debug(`Ignoring ${this.constructor.name} related fields since it is an invalid section`);
    }

    this._cacheValue = {
      allFieldNames: null,
      matchingSelectOption: null,
    };
  }

  /*
   * Examine the section is a valid section or not based on its fieldDetails or
   * other information. This method must be overrided.
   *
   * @returns {boolean} True for a valid section, otherwise false
   *
   */
  isValidSection() {
    throw new TypeError("isValidSection method must be overrided");
  }

  /*
   * Examine the section is an enabled section type or not based on its
   * preferences. This method must be overrided.
   *
   * @returns {boolean} True for an enabled section type, otherwise false
   *
   */
  isEnabled() {
    throw new TypeError("isEnabled method must be overrided");
  }

  /*
   * Examine the section is createable for storing the profile. This method
   * must be overrided.
   *
   * @param {Object} record The record for examining createable
   * @returns {boolean} True for the record is createable, otherwise false
   *
   */
  isRecordCreatable(record) {
    throw new TypeError("isRecordCreatable method must be overrided");
  }

  /**
   * Override this method if the profile is needed to apply some transformers.
   *
   * @param {Object} profile
   *        A profile should be converted based on the specific requirement.
   */
  applyTransformers(profile) {}

  /**
   * Override this method if the profile is needed to be customized for
   * previewing values.
   *
   * @param {Object} profile
   *        A profile for pre-processing before previewing values.
   */
  preparePreviewProfile(profile) {}

  /**
   * Override this method if the profile is needed to be customized for filling
   * values.
   *
   * @param {Object} profile
   *        A profile for pre-processing before filling values.
   */
  async prepareFillingProfile(profile) {}

  /*
   * Override this methid if any data for `createRecord` is needed to be
   * normailized before submitting the record.
   *
   * @param {Object} profile
   *        A record for normalization.
   */
  normalizeCreatingRecord(data) {}

  /*
   * Override this method if there is any field value needs to compute for a
   * specific case. Return the original value in the default case.
   * @param {String} value
   *        The original field value.
   * @param {Object} fieldDetail
   *        A fieldDetail of the related element.
   * @param {HTMLElement} element
   *        A element for checking converting value.
   *
   * @returns {String}
   *          A string of the converted value.
   */
  computeFillingValue(value, fieldName, element) {
    return value;
  }

  set focusedInput(element) {
    this._focusedDetail = this.getFieldDetailByElement(element);
  }

  getFieldDetailByElement(element) {
    return this.fieldDetails.find(
      detail => detail.elementWeakRef.get() == element
    );
  }

  get allFieldNames() {
    if (!this._cacheValue.allFieldNames) {
      this._cacheValue.allFieldNames = this.fieldDetails.map(record => record.fieldName);
    }
    return this._cacheValue.allFieldNames;
  }

  getFieldDetailByName(fieldName) {
    return this.fieldDetails.find(detail => detail.fieldName == fieldName);
  }

  matchSelectOptions(profile) {
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

  adaptFieldMaxLength(profile) {
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
      this.applyTransformers(profile);
    }
    return originalProfiles;
  }

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   */
  async autofillFields(profile) {
    let focusedDetail = this._focusedDetail;
    if (!focusedDetail) {
      throw new Error("No fieldDetail for the focused input.");
    }

    await this.prepareFillingProfile(profile);
    log.debug("profile in autofillFields:", profile);

    this.filledRecordGUID = profile.guid;
    for (let fieldDetail of this.fieldDetails) {
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
        let focusedInput = focusedDetail.elementWeakRef.get();
        if (element == focusedInput ||
            (element != focusedInput && !element.value)) {
          element.setUserInput(value);
          this._changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
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
        this._changeFieldState(fieldDetail, FIELD_STATES.AUTO_FILLED);
      }
    }
  }

  /**
   * Populates result to the preview layers with given profile.
   *
   * @param {Object} profile
   *        A profile to be previewed with
   */
  previewFormFields(profile) {
    log.debug("preview profile: ", profile);

    this.preparePreviewProfile(profile);

    for (let fieldDetail of this.fieldDetails) {
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
      this._changeFieldState(fieldDetail, value ? FIELD_STATES.PREVIEW : FIELD_STATES.NORMAL);
    }
  }

  /**
   * Clear preview text and background highlight of all fields.
   */
  clearPreviewedFormFields() {
    log.debug("clear previewed fields in:", this.form);

    for (let fieldDetail of this.fieldDetails) {
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

      this._changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
  }

  /**
   * Clear value and highlight style of all filled fields.
   */
  clearPopulatedForm() {
    for (let fieldDetail of this.fieldDetails) {
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
  }

  /**
   * Change the state of a field to correspond with different presentations.
   *
   * @param {Object} fieldDetail
   *        A fieldDetail of which its element is about to update the state.
   * @param {string} nextState
   *        Used to determine the next state
   */
  _changeFieldState(fieldDetail, nextState) {
    let element = fieldDetail.elementWeakRef.get();

    if (!element) {
      log.warn(fieldDetail.fieldName, "is unreachable while changing state");
      return;
    }
    if (!(nextState in this._FIELD_STATE_ENUM)) {
      log.warn(fieldDetail.fieldName, "is trying to change to an invalid state");
      return;
    }
    if (fieldDetail.state == nextState) {
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

    switch (nextState) {
      case FIELD_STATES.NORMAL: {
        if (fieldDetail.state == FIELD_STATES.AUTO_FILLED) {
          element.removeEventListener("input", this, {mozSystemGroup: true});
        }
        break;
      }
      case FIELD_STATES.AUTO_FILLED: {
        element.addEventListener("input", this, {mozSystemGroup: true});
        break;
      }
    }

    fieldDetail.state = nextState;
  }

  resetFieldStates() {
    for (let fieldDetail of this.fieldDetails) {
      const element = fieldDetail.elementWeakRef.get();
      element.removeEventListener("input", this, {mozSystemGroup: true});
      this._changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
    }
    this.filledRecordGUID = null;
  }

  isFilled() {
    return !!this.filledRecordGUID;
  }

  /**
   * Return the record that is converted from `fieldDetails` and only valid
   * form record is included.
   *
   * @returns {Object|null}
   *          A record object consists of three properties:
   *            - guid: The id of the previously-filled profile or null if omitted.
   *            - record: A valid record converted from details with trimmed result.
   *            - untouchedFields: Fields that aren't touched after autofilling.
   *          Return `null` for any uncreatable or invalid record.
   */
  createRecord() {
    let details = this.fieldDetails;
    if (!this.isEnabled() || !details || details.length == 0) {
      return null;
    }

    let data = {
      guid: this.filledRecordGUID,
      record: {},
      untouchedFields: [],
    };

    details.forEach(detail => {
      let element = detail.elementWeakRef.get();
      // Remove the unnecessary spaces
      let value = element && element.value.trim();
      value = this.computeFillingValue(value, detail, element);

      if (!value || value.length > FormAutofillUtils.MAX_FIELD_VALUE_LENGTH) {
        // Keep the property and preserve more information for updating
        data.record[detail.fieldName] = "";
        return;
      }

      data.record[detail.fieldName] = value;

      if (detail.state == FIELD_STATES.AUTO_FILLED) {
        data.untouchedFields.push(detail.fieldName);
      }
    });

    this.normalizeCreatingRecord(data);

    if (!this.isRecordCreatable(data.record)) {
      return null;
    }

    return data;
  }

  handleEvent(event) {
    switch (event.type) {
      case "input": {
        if (!event.isTrusted) {
          return;
        }
        const target = event.target;
        const targetFieldDetail = this.getFieldDetailByElement(target);

        this._changeFieldState(targetFieldDetail, FIELD_STATES.NORMAL);

        let isAutofilled = false;
        let dimFieldDetails = [];
        for (const fieldDetail of this.fieldDetails) {
          const element = fieldDetail.elementWeakRef.get();

          if (ChromeUtils.getClassName(element) === "HTMLSelectElement") {
            // Dim fields are those we don't attempt to revert their value
            // when clear the target set, such as <select>.
            dimFieldDetails.push(fieldDetail);
          } else {
            isAutofilled |= fieldDetail.state == FIELD_STATES.AUTO_FILLED;
          }
        }
        if (!isAutofilled) {
          // Restore the dim fields to initial state as well once we knew
          // that user had intention to clear the filled form manually.
          for (const fieldDetail of dimFieldDetails) {
            this._changeFieldState(fieldDetail, FIELD_STATES.NORMAL);
          }
          this.filledRecordGUID = null;
        }
        break;
      }
    }
  }
}

class FormAutofillAddressSection extends FormAutofillSection {
  constructor(fieldDetails, winUtils) {
    super(fieldDetails, winUtils);

    this._cacheValue.oneLineStreetAddress = null;
  }

  isValidSection() {
    return this.fieldDetails.length >= FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD;
  }

  isEnabled() {
    return FormAutofillUtils.isAutofillAddressesEnabled;
  }

  isRecordCreatable(record) {
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

  _getOneLineStreetAddress(address) {
    if (!this._cacheValue.oneLineStreetAddress) {
      this._cacheValue.oneLineStreetAddress = {};
    }
    if (!this._cacheValue.oneLineStreetAddress[address]) {
      this._cacheValue.oneLineStreetAddress[address] = FormAutofillUtils.toOneLineAddress(address);
    }
    return this._cacheValue.oneLineStreetAddress[address];
  }

  addressTransformer(profile) {
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
  telTransformer(profile) {
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

  /*
   * Apply all address related transformers.
   *
   * @param {Object} profile
   *        A profile for adjusting address related value.
   * @override
   */
  applyTransformers(profile) {
    this.addressTransformer(profile);
    this.telTransformer(profile);
    this.matchSelectOptions(profile);
    this.adaptFieldMaxLength(profile);
  }

  computeFillingValue(value, fieldDetail, element) {
    // Try to abbreviate the value of select element.
    if (fieldDetail.fieldName == "address-level1" &&
      ChromeUtils.getClassName(element) === "HTMLSelectElement") {
      // Don't save the record when the option value is empty *OR* there
      // are multiple options being selected. The empty option is usually
      // assumed to be default along with a meaningless text to users.
      if (!value || element.selectedOptions.length != 1) {
        // Keep the property and preserve more information for address updating
        value = "";
      } else {
        let text = element.selectedOptions[0].text.trim();
        value = FormAutofillUtils.getAbbreviatedSubregionName([value, text]) || text;
      }
    }
    return value;
  }

  normalizeCreatingRecord(address) {
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
}

class FormAutofillCreditCardSection extends FormAutofillSection {
  constructor(fieldDetails, winUtils) {
    super(fieldDetails, winUtils);
  }

  isValidSection() {
    let ccNumberReason = "";
    let hasCCNumber = false;
    let hasExpiryDate = false;
    let hasCCName = false;

    for (let detail of this.fieldDetails) {
      switch (detail.fieldName) {
        case "cc-number":
          hasCCNumber = true;
          ccNumberReason = detail._reason;
          break;
        case "cc-name":
        case "cc-given-name":
        case "cc-additional-name":
        case "cc-family-name":
          hasCCName = true;
          break;
        case "cc-exp":
        case "cc-exp-month":
        case "cc-exp-year":
          hasExpiryDate = true;
          break;
      }
    }

    return hasCCNumber && (ccNumberReason == "autocomplete" || hasExpiryDate || hasCCName);
  }

  isEnabled() {
    return FormAutofillUtils.isAutofillCreditCardsEnabled;
  }

  isRecordCreatable(record) {
    return record["cc-number"] && FormAutofillUtils.isCCNumber(record["cc-number"]);
  }

  creditCardExpDateTransformer(profile) {
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

  async _decrypt(cipherText, reauth) {
    return new Promise((resolve) => {
      Services.cpmm.addMessageListener("FormAutofill:DecryptedString", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:DecryptedString", getResult);
        resolve(result.data);
      });

      Services.cpmm.sendAsyncMessage("FormAutofill:GetDecryptedString", {cipherText, reauth});
    });
  }

  /*
   * Apply all credit card related transformers.
   *
   * @param {Object} profile
   *        A profile for adjusting credit card related value.
   * @override
   */
  applyTransformers(profile) {
    this.matchSelectOptions(profile);
    this.creditCardExpDateTransformer(profile);
    this.adaptFieldMaxLength(profile);
  }

  /**
   * Customize for previewing prorifle.
   *
   * @param {Object} profile
   *        A profile for pre-processing before previewing values.
   * @override
   */
  preparePreviewProfile(profile) {
    // Always show the decrypted credit card number when Master Password is
    // disabled.
    if (profile["cc-number-decrypted"]) {
      profile["cc-number"] = profile["cc-number-decrypted"];
    }
  }

  /**
   * Customize for filling prorifle.
   *
   * @param {Object} profile
   *        A profile for pre-processing before filling values.
   * @override
   */
  async prepareFillingProfile(profile) {
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

  set focusedInput(element) {
    let section = this._sectionCache.get(element);
    if (!section) {
      section = this.sections.find(
        s => s.getFieldDetailByElement(element)
      );
      this._sectionCache.set(element, section);
    }

    this._focusedSection = section;

    if (section) {
      section.focusedInput = element;
    }
  }

  get activeSection() {
    return this._focusedSection;
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
    for (let {fieldDetails, type} of sections) {
      let section;
      if (type == FormAutofillUtils.SECTION_TYPES.ADDRESS) {
        section = new FormAutofillAddressSection(fieldDetails, this.winUtils);
      } else if (type == FormAutofillUtils.SECTION_TYPES.CREDIT_CARD) {
        section = new FormAutofillCreditCardSection(fieldDetails, this.winUtils);
      } else {
        throw new Error("Unknown field type.");
      }
      this.sections.push(section);
      allValidDetails.push(...section.fieldDetails);
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

  _hasFilledSection() {
    return this.sections.some(section => section.isFilled());
  }

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   */
  async autofillFormFields(profile) {
    let noFilledSectionsPreviously = !this._hasFilledSection();
    await this.activeSection.autofillFields(profile);

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
      if (!this._hasFilledSection()) {
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
      const secRecord = section.createRecord();
      if (!secRecord) {
        continue;
      }
      if (section instanceof FormAutofillAddressSection) {
        records.address.push(secRecord);
      } else if (section instanceof FormAutofillCreditCardSection) {
        records.creditCard.push(secRecord);
      } else {
        throw new Error("Unknown section type");
      }
    }
    log.debug("Create records:", records);
    return records;
  }
}
