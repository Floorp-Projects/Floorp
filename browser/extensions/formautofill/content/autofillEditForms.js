/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported EditAddress, EditCreditCard */
/* eslint-disable mozilla/balanced-listeners */ // Not relevant since the document gets unloaded.

"use strict";

const { FormAutofill } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofill.sys.mjs"
);
const { FormAutofillUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillUtils.sys.mjs"
);

class EditAutofillForm {
  constructor(elements) {
    this._elements = elements;
  }

  /**
   * Fill the form with a record object.
   *
   * @param  {object} [record = {}]
   */
  loadRecord(record = {}) {
    for (let field of this._elements.form.elements) {
      let value = record[field.id];
      value = typeof value == "undefined" ? "" : value;

      if (record.guid) {
        field.value = value;
      } else if (field.localName == "select") {
        this.setDefaultSelectedOptionByValue(field, value);
      } else {
        // Use .defaultValue instead of .value to avoid setting the `dirty` flag
        // which triggers form validation UI.
        field.defaultValue = value;
      }
    }
    if (!record.guid) {
      // Reset the dirty value flag and validity state.
      this._elements.form.reset();
    } else {
      for (let field of this._elements.form.elements) {
        this.updatePopulatedState(field);
        this.updateCustomValidity(field);
      }
    }
  }

  setDefaultSelectedOptionByValue(select, value) {
    for (let option of select.options) {
      option.defaultSelected = option.value == value;
    }
  }

  /**
   * Get a record from the form suitable for a save/update in storage.
   *
   * @returns {object}
   */
  buildFormObject() {
    let initialObject = {};
    if (this.hasMailingAddressFields) {
      // Start with an empty string for each mailing-address field so that any
      // fields hidden for the current country are blanked in the return value.
      initialObject = {
        "street-address": "",
        "address-level3": "",
        "address-level2": "",
        "address-level1": "",
        "postal-code": "",
      };
    }

    return Array.from(this._elements.form.elements).reduce((obj, input) => {
      if (!input.disabled) {
        obj[input.id] = input.value;
      }
      return obj;
    }, initialObject);
  }

  /**
   * Handle events
   *
   * @param  {DOMEvent} event
   */
  handleEvent(event) {
    switch (event.type) {
      case "change": {
        this.handleChange(event);
        break;
      }
      case "input": {
        this.handleInput(event);
        break;
      }
    }
  }

  /**
   * Handle change events
   *
   * @param  {DOMEvent} event
   */
  handleChange(event) {
    this.updatePopulatedState(event.target);
  }

  /**
   * Handle input events
   *
   * @param  {DOMEvent} event
   */
  handleInput(event) {}

  /**
   * Attach event listener
   */
  attachEventListeners() {
    this._elements.form.addEventListener("input", this);
  }

  /**
   * Set the field-populated attribute if the field has a value.
   *
   * @param {DOMElement} field The field that will be checked for a value.
   */
  updatePopulatedState(field) {
    let span = field.parentNode.querySelector(".label-text");
    if (!span) {
      return;
    }
    span.toggleAttribute("field-populated", !!field.value.trim());
  }

  /**
   * Run custom validity routines specific to the field and type of form.
   *
   * @param {DOMElement} field The field that will be validated.
   */
  updateCustomValidity(field) {}
}

class EditAddress extends EditAutofillForm {
  /**
   * @param {HTMLElement[]} elements
   * @param {object} record
   * @param {object} config
   * @param {boolean} [config.noValidate=undefined] Whether to validate the form
   */
  constructor(elements, record, config) {
    super(elements);

    Object.assign(this, config);
    let { form } = this._elements;
    Object.assign(this._elements, {
      addressLevel3Label: form.querySelector(
        "#address-level3-container > .label-text"
      ),
      addressLevel2Label: form.querySelector(
        "#address-level2-container > .label-text"
      ),
      addressLevel1Label: form.querySelector(
        "#address-level1-container > .label-text"
      ),
      postalCodeLabel: form.querySelector(
        "#postal-code-container > .label-text"
      ),
      country: form.querySelector("#country"),
    });

    this.populateCountries();
    // Need to populate the countries before trying to set the initial country.
    // Also need to use this._record so it has the default country selected.
    this.loadRecord(record);
    this.attachEventListeners();

    form.noValidate = !!config.noValidate;
  }

  loadRecord(record) {
    this._record = record;
    if (!record) {
      record = {
        country: FormAutofill.DEFAULT_REGION,
      };
    }

    let { addressLevel1Options } = FormAutofillUtils.getFormFormat(
      record.country
    );
    this.populateAddressLevel1(addressLevel1Options, record.country);

    super.loadRecord(record);
    this.loadAddressLevel1(record["address-level1"], record.country);
    this.formatForm(record.country);
  }

  get hasMailingAddressFields() {
    let { addressFields } = this._elements.form.dataset;
    return (
      !addressFields ||
      addressFields.trim().split(/\s+/).includes("mailing-address")
    );
  }

  /**
   * `mailing-address` is a special attribute token to indicate mailing fields + country.
   *
   * @param {object[]} mailingFieldsOrder - `fieldsOrder` from `getFormFormat`
   * @param {string} addressFields - white-space-separated string of requested address fields to show
   * @returns {object[]} in the same structure as `mailingFieldsOrder` but including non-mail fields
   */
  static computeVisibleFields(mailingFieldsOrder, addressFields) {
    if (addressFields) {
      let requestedFieldClasses = addressFields.trim().split(/\s+/);
      let fieldClasses = [];
      if (requestedFieldClasses.includes("mailing-address")) {
        fieldClasses = fieldClasses.concat(mailingFieldsOrder);
        // `country` isn't part of the `mailingFieldsOrder` so add it when filling a mailing-address
        requestedFieldClasses.splice(
          requestedFieldClasses.indexOf("mailing-address"),
          1,
          "country"
        );
      }

      for (let fieldClassName of requestedFieldClasses) {
        fieldClasses.push({
          fieldId: fieldClassName,
          newLine: fieldClassName == "name",
        });
      }
      return fieldClasses;
    }

    // This is the default which is shown in the management interface and includes all fields.
    return mailingFieldsOrder.concat([
      {
        fieldId: "country",
      },
      {
        fieldId: "tel",
      },
      {
        fieldId: "email",
        newLine: true,
      },
    ]);
  }

  /**
   * Format the form based on country. The address-level1 and postal-code labels
   * should be specific to the given country.
   *
   * @param  {string} country
   */
  formatForm(country) {
    const {
      addressLevel3L10nId,
      addressLevel2L10nId,
      addressLevel1L10nId,
      addressLevel1Options,
      postalCodeL10nId,
      fieldsOrder: mailingFieldsOrder,
      postalCodePattern,
      countryRequiredFields,
    } = FormAutofillUtils.getFormFormat(country);

    document.l10n.setAttributes(
      this._elements.addressLevel3Label,
      addressLevel3L10nId
    );
    document.l10n.setAttributes(
      this._elements.addressLevel2Label,
      addressLevel2L10nId
    );
    document.l10n.setAttributes(
      this._elements.addressLevel1Label,
      addressLevel1L10nId
    );
    document.l10n.setAttributes(
      this._elements.postalCodeLabel,
      postalCodeL10nId
    );
    let addressFields = this._elements.form.dataset.addressFields;
    let extraRequiredFields = this._elements.form.dataset.extraRequiredFields;
    let fieldClasses = EditAddress.computeVisibleFields(
      mailingFieldsOrder,
      addressFields
    );
    let requiredFields = new Set(countryRequiredFields);
    if (extraRequiredFields) {
      for (let extraRequiredField of extraRequiredFields.trim().split(/\s+/)) {
        requiredFields.add(extraRequiredField);
      }
    }
    this.arrangeFields(fieldClasses, requiredFields);
    this.updatePostalCodeValidation(postalCodePattern);
    this.populateAddressLevel1(addressLevel1Options, country);
  }

  /**
   * Update address field visibility and order based on libaddressinput data.
   *
   * @param {object[]} fieldsOrder array of objects with `fieldId` and optional `newLine` properties
   * @param {Set} requiredFields Set of `fieldId` strings that mark which fields are required
   */
  arrangeFields(fieldsOrder, requiredFields) {
    /**
     * @see FormAutofillStorage.VALID_ADDRESS_FIELDS
     */
    let fields = [
      // `name` is a wrapper for the 3 name fields.
      "name",
      "organization",
      "street-address",
      "address-level3",
      "address-level2",
      "address-level1",
      "postal-code",
      "country",
      "tel",
      "email",
    ];
    let inputs = [];
    for (let i = 0; i < fieldsOrder.length; i++) {
      let { fieldId, newLine } = fieldsOrder[i];

      let container = this._elements.form.querySelector(
        `#${fieldId}-container`
      );
      let containerInputs = [
        ...container.querySelectorAll("input, textarea, select"),
      ];
      containerInputs.forEach(function (input) {
        input.disabled = false;
        // libaddressinput doesn't list 'country' or 'name' as required.
        input.required =
          fieldId == "country" ||
          fieldId == "name" ||
          requiredFields.has(fieldId);
      });
      inputs.push(...containerInputs);
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
      let container = this._elements.form.querySelector(`#${field}-container`);
      container.style.display = "none";
      for (let input of [
        ...container.querySelectorAll("input, textarea, select"),
      ]) {
        input.disabled = true;
      }
    }
  }

  updatePostalCodeValidation(postalCodePattern) {
    let postalCodeInput = this._elements.form.querySelector("#postal-code");
    if (postalCodePattern && postalCodeInput.style.display != "none") {
      postalCodeInput.setAttribute("pattern", postalCodePattern);
    } else {
      postalCodeInput.removeAttribute("pattern");
    }
  }

  /**
   * Set the address-level1 value on the form field (input or select, whichever is present).
   *
   * @param {string} addressLevel1Value Value of the address-level1 from the autofill record
   * @param {string} country The corresponding country
   */
  loadAddressLevel1(addressLevel1Value, country) {
    let field = this._elements.form.querySelector("#address-level1");

    if (field.localName == "input") {
      field.value = addressLevel1Value || "";
      return;
    }

    let matchedSelectOption = FormAutofillUtils.findAddressSelectOption(
      field,
      {
        country,
        "address-level1": addressLevel1Value,
      },
      "address-level1"
    );
    if (matchedSelectOption && !matchedSelectOption.selected) {
      field.value = matchedSelectOption.value;
      field.dispatchEvent(new Event("input", { bubbles: true }));
      field.dispatchEvent(new Event("change", { bubbles: true }));
    } else if (addressLevel1Value) {
      // If the option wasn't found, insert an option at the beginning of
      // the select that matches the stored value.
      field.insertBefore(
        new Option(addressLevel1Value, addressLevel1Value, true, true),
        field.firstChild
      );
    }
  }

  /**
   * Replace the text input for address-level1 with a select dropdown if
   * a fixed set of names exists. Otherwise show a text input.
   *
   * @param {Map?} options Map of options with regionCode -> name mappings
   * @param {string} country The corresponding country
   */
  populateAddressLevel1(options, country) {
    let field = this._elements.form.querySelector("#address-level1");

    if (field.dataset.country == country) {
      return;
    }

    if (!options) {
      if (field.localName == "input") {
        return;
      }

      let input = document.createElement("input");
      input.setAttribute("type", "text");
      input.id = "address-level1";
      input.required = field.required;
      input.disabled = field.disabled;
      input.tabIndex = field.tabIndex;
      field.replaceWith(input);
      return;
    }

    if (field.localName == "input") {
      let select = document.createElement("select");
      select.id = "address-level1";
      select.required = field.required;
      select.disabled = field.disabled;
      select.tabIndex = field.tabIndex;
      field.replaceWith(select);
      field = select;
    }

    field.textContent = "";
    field.dataset.country = country;
    let fragment = document.createDocumentFragment();
    fragment.appendChild(new Option(undefined, undefined, true, true));
    for (let [regionCode, regionName] of options) {
      let option = new Option(regionName, regionCode);
      fragment.appendChild(option);
    }
    field.appendChild(fragment);
  }

  populateCountries() {
    let fragment = document.createDocumentFragment();
    // Sort countries by their visible names.
    let countries = [...FormAutofill.countries.entries()].sort((e1, e2) =>
      e1[1].localeCompare(e2[1])
    );
    for (let [country] of countries) {
      const countryName = Services.intl.getRegionDisplayNames(undefined, [
        country.toLowerCase(),
      ]);
      const option = new Option(countryName, country);
      fragment.appendChild(option);
    }
    this._elements.country.appendChild(fragment);
  }

  handleChange(event) {
    if (event.target == this._elements.country) {
      this.formatForm(event.target.value);
    }
    super.handleChange(event);
  }

  attachEventListeners() {
    this._elements.form.addEventListener("change", this);
    super.attachEventListeners();
  }
}

class EditCreditCard extends EditAutofillForm {
  /**
   * @param {HTMLElement[]} elements
   * @param {object} record with a decrypted cc-number
   * @param {object} addresses in an object with guid keys for the billing address picker.
   */
  constructor(elements, record, addresses) {
    super(elements);

    this._addresses = addresses;
    Object.assign(this._elements, {
      ccNumber: this._elements.form.querySelector("#cc-number"),
      invalidCardNumberStringElement: this._elements.form.querySelector(
        "#invalidCardNumberString"
      ),
      month: this._elements.form.querySelector("#cc-exp-month"),
      year: this._elements.form.querySelector("#cc-exp-year"),
      billingAddress: this._elements.form.querySelector("#billingAddressGUID"),
      billingAddressRow:
        this._elements.form.querySelector(".billingAddressRow"),
    });

    this.attachEventListeners();
    this.loadRecord(record, addresses);
  }

  loadRecord(record, addresses, preserveFieldValues) {
    // _record must be updated before generateYears and generateBillingAddressOptions are called.
    this._record = record;
    this._addresses = addresses;
    this.generateBillingAddressOptions(preserveFieldValues);
    if (!preserveFieldValues) {
      // Re-generating the months will reset the selected option.
      this.generateMonths();
      // Re-generating the years will reset the selected option.
      this.generateYears();
      super.loadRecord(record);
    }
  }

  generateMonths() {
    const count = 12;

    // Clear the list
    this._elements.month.textContent = "";

    // Empty month option
    this._elements.month.appendChild(new Option());

    // Populate month list. Format: "month number - month name"
    let dateFormat = new Intl.DateTimeFormat(navigator.language, {
      month: "long",
    }).format;
    for (let i = 0; i < count; i++) {
      let monthNumber = (i + 1).toString();
      let monthName = dateFormat(new Date(1970, i));
      let option = new Option();
      option.value = monthNumber;
      // XXX: Bug 1446164 - Localize this string.
      option.textContent = `${monthNumber.padStart(2, "0")} - ${monthName}`;
      this._elements.month.appendChild(option);
    }
  }

  generateYears() {
    const count = 11;
    const currentYear = new Date().getFullYear();
    const ccExpYear = this._record && this._record["cc-exp-year"];

    // Clear the list
    this._elements.year.textContent = "";

    // Provide an empty year option
    this._elements.year.appendChild(new Option());

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

  generateBillingAddressOptions(preserveFieldValues) {
    let billingAddressGUID;
    if (preserveFieldValues && this._elements.billingAddress.value) {
      billingAddressGUID = this._elements.billingAddress.value;
    } else if (this._record) {
      billingAddressGUID = this._record.billingAddressGUID;
    }

    this._elements.billingAddress.textContent = "";

    this._elements.billingAddress.appendChild(new Option("", ""));

    let hasAddresses = false;
    for (let [guid, address] of Object.entries(this._addresses)) {
      hasAddresses = true;
      let selected = guid == billingAddressGUID;
      let option = new Option(
        FormAutofillUtils.getAddressLabel(address),
        guid,
        selected,
        selected
      );
      this._elements.billingAddress.appendChild(option);
    }

    this._elements.billingAddressRow.hidden = !hasAddresses;
  }

  attachEventListeners() {
    this._elements.form.addEventListener("change", this);
    super.attachEventListeners();
  }

  handleInput(event) {
    // Clear the error message if cc-number is valid
    if (
      event.target == this._elements.ccNumber &&
      FormAutofillUtils.isCCNumber(this._elements.ccNumber.value)
    ) {
      this._elements.ccNumber.setCustomValidity("");
    }
    super.handleInput(event);
  }

  updateCustomValidity(field) {
    super.updateCustomValidity(field);

    // Mark the cc-number field as invalid if the number is empty or invalid.
    if (
      field == this._elements.ccNumber &&
      !FormAutofillUtils.isCCNumber(field.value)
    ) {
      let invalidCardNumberString =
        this._elements.invalidCardNumberStringElement.textContent;
      field.setCustomValidity(invalidCardNumberString || " ");
    }
  }
}
