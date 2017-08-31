/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const ADDRESS_REFERENCES = "chrome://formautofill/content/addressReferences.js";

// TODO: We only support US in MVP. We are going to support more countries in
//       bug 1370193.
const ALTERNATIVE_COUNTRY_NAMES = {
  "US": ["US", "United States of America", "United States", "America", "U.S.", "USA", "U.S.A.", "U.S.A"],
};

const ADDRESSES_COLLECTION_NAME = "addresses";
const CREDITCARDS_COLLECTION_NAME = "creditCards";
const ENABLED_AUTOFILL_ADDRESSES_PREF = "extensions.formautofill.addresses.enabled";
const ENABLED_AUTOFILL_CREDITCARDS_PREF = "extensions.formautofill.creditCards.enabled";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.FormAutofillUtils = {
  get AUTOFILL_FIELDS_THRESHOLD() { return 3; },
  get isAutofillEnabled() { return this.isAutofillAddressesEnabled || this.isAutofillCreditCardsEnabled; },

  ADDRESSES_COLLECTION_NAME,
  CREDITCARDS_COLLECTION_NAME,
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,

  _fieldNameInfo: {
    "name": "name",
    "given-name": "name",
    "additional-name": "name",
    "family-name": "name",
    "organization": "organization",
    "street-address": "address",
    "address-line1": "address",
    "address-line2": "address",
    "address-line3": "address",
    "address-level1": "address",
    "address-level2": "address",
    "postal-code": "address",
    "country": "address",
    "country-name": "address",
    "tel": "tel",
    "tel-country-code": "tel",
    "tel-national": "tel",
    "tel-area-code": "tel",
    "tel-local": "tel",
    "tel-local-prefix": "tel",
    "tel-local-suffix": "tel",
    "tel-extension": "tel",
    "email": "email",
    "cc-name": "creditCard",
    "cc-given-name": "creditCard",
    "cc-additional-name": "creditCard",
    "cc-family-name": "creditCard",
    "cc-number": "creditCard",
    "cc-exp-month": "creditCard",
    "cc-exp-year": "creditCard",
    "cc-exp": "creditCard",
  },
  _addressDataLoaded: false,
  _collators: {},
  _reAlternativeCountryNames: {},

  isAddressField(fieldName) {
    return !!this._fieldNameInfo[fieldName] && !this.isCreditCardField(fieldName);
  },

  isCreditCardField(fieldName) {
    return this._fieldNameInfo[fieldName] == "creditCard";
  },

  isCCNumber(ccNumber) {
    // Based on the information on wiki[1], the shortest valid length should be
    // 12 digits(Maestro).
    // [1] https://en.wikipedia.org/wiki/Payment_card_number
    return ccNumber ? ccNumber.replace(/\s/g, "").match(/^\d{12,}$/) : false;
  },

  getCategoryFromFieldName(fieldName) {
    return this._fieldNameInfo[fieldName];
  },

  getCategoriesFromFieldNames(fieldNames) {
    let categories = new Set();
    for (let fieldName of fieldNames) {
      let info = this.getCategoryFromFieldName(fieldName);
      if (info) {
        categories.add(info);
      }
    }
    return Array.from(categories);
  },

  getAddressSeparator() {
    // The separator should be based on the L10N address format, and using a
    // white space is a temporary solution.
    return " ";
  },

  toOneLineAddress(address, delimiter = "\n") {
    let array = typeof address == "string" ? address.split(delimiter) : address;

    if (!Array.isArray(array)) {
      return "";
    }
    return array
      .map(s => s ? s.trim() : "")
      .filter(s => s)
      .join(this.getAddressSeparator());
  },

  fmtMaskedCreditCardLabel(maskedCCNum = "") {
    return {
      affix: "****",
      label: maskedCCNum.replace(/^\**/, ""),
    };
  },

  defineLazyLogGetter(scope, logPrefix) {
    XPCOMUtils.defineLazyGetter(scope, "log", () => {
      let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
      return new ConsoleAPI({
        maxLogLevelPref: "extensions.formautofill.loglevel",
        prefix: logPrefix,
      });
    });
  },

  autofillFieldSelector(doc) {
    return doc.querySelectorAll("input, select");
  },

  ALLOWED_TYPES: ["text", "email", "tel", "number"],
  isFieldEligibleForAutofill(element) {
    if (element.autocomplete == "off") {
      return false;
    }

    let tagName = element.tagName;
    if (tagName == "INPUT") {
      // `element.type` can be recognized as `text`, if it's missing or invalid.
      if (!this.ALLOWED_TYPES.includes(element.type)) {
        return false;
      }
    } else if (tagName != "SELECT") {
      return false;
    }

    return true;
  },

  loadDataFromScript(url, sandbox = {}) {
    let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                         .getService(Ci.mozIJSSubScriptLoader);
    scriptLoader.loadSubScript(url, sandbox, "utf-8");
    return sandbox;
  },

  /**
   * Get country address data. Fallback to US if not found.
   * @param   {string} country
   * @returns {object}
   */
  getCountryAddressData(country) {
    // Load the addressData if needed
    if (!this._addressDataLoaded) {
      Object.assign(this, this.loadDataFromScript(ADDRESS_REFERENCES));
      this._addressDataLoaded = true;
    }
    return this.addressData[`data/${country}`] || this.addressData["data/US"];
  },

  /**
   * Get the collators based on the specified country.
   * @param   {string} country The specified country.
   * @returns {array} An array containing several collator objects.
   */
  getCollators(country) {
    // TODO: Only one language should be used at a time per country. The locale
    //       of the page should be taken into account to do this properly.
    //       We are going to support more countries in bug 1370193 and this
    //       should be addressed when we start to implement that bug.

    if (!this._collators[country]) {
      let dataset = this.getCountryAddressData(country);
      let languages = dataset.languages ? dataset.languages.split("~") : [dataset.lang];
      this._collators[country] = languages.map(lang => new Intl.Collator(lang, {sensitivity: "base", ignorePunctuation: true}));
    }
    return this._collators[country];
  },

  /**
   * Use alternative country name list to identify a country code from a
   * specified country name.
   * @param   {string} countryName A country name to be identified
   * @param   {string} [countrySpecified] A country code indicating that we only
   *                                      search its alternative names if specified.
   * @returns {string} The matching country code.
   */
  identifyCountryCode(countryName, countrySpecified) {
    let countries = countrySpecified ? [countrySpecified] : Object.keys(ALTERNATIVE_COUNTRY_NAMES);

    for (let country of countries) {
      let collators = this.getCollators(country);

      let alternativeCountryNames = ALTERNATIVE_COUNTRY_NAMES[country];
      let reAlternativeCountryNames = this._reAlternativeCountryNames[country];
      if (!reAlternativeCountryNames) {
        reAlternativeCountryNames = this._reAlternativeCountryNames[country] = [];
      }

      for (let i = 0; i < alternativeCountryNames.length; i++) {
        let name = alternativeCountryNames[i];
        let reName = reAlternativeCountryNames[i];
        if (!reName) {
          reName = reAlternativeCountryNames[i] = new RegExp("\\b" + this.escapeRegExp(name) + "\\b", "i");
        }

        if (this.strCompare(name, countryName, collators) || reName.test(countryName)) {
          return country;
        }
      }
    }

    return null;
  },

  findSelectOption(selectEl, record, fieldName) {
    if (this.isAddressField(fieldName)) {
      return this.findAddressSelectOption(selectEl, record, fieldName);
    }
    if (this.isCreditCardField(fieldName)) {
      return this.findCreditCardSelectOption(selectEl, record, fieldName);
    }
    return null;
  },

  /**
   * Try to find the abbreviation of the given state name
   * @param   {string[]} stateValues A list of inferable state values.
   * @param   {string} country A country name to be identified.
   * @returns {string} The matching state abbreviation.
   */
  getAbbreviatedStateName(stateValues, country = this.DEFAULT_COUNTRY_CODE) {
    let values = Array.isArray(stateValues) ? stateValues : [stateValues];

    let collators = this.getCollators(country);
    let {sub_keys: subKeys, sub_names: subNames} = this.getCountryAddressData(country);

    if (!Array.isArray(subKeys)) {
      subKeys = subKeys.split("~");
    }
    if (!Array.isArray(subNames)) {
      subNames = subNames.split("~");
    }

    let speculatedSubIndexes = [];
    for (const val of values) {
      let identifiedValue = this.identifyValue(subKeys, subNames, val, collators);
      if (identifiedValue) {
        return identifiedValue;
      }

      // Predict the possible state by partial-matching if no exact match.
      [subKeys, subNames].forEach(sub => {
        speculatedSubIndexes.push(sub.findIndex(token => {
          let pattern = new RegExp("\\b" + this.escapeRegExp(token) + "\\b");

          return pattern.test(val);
        }));
      });
    }

    return subKeys[speculatedSubIndexes.find(i => !!~i)] || null;
  },

  /**
   * Find the option element from select element.
   * 1. Try to find the locale using the country from address.
   * 2. First pass try to find exact match.
   * 3. Second pass try to identify values from address value and options,
   *    and look for a match.
   * @param   {DOMElement} selectEl
   * @param   {object} address
   * @param   {string} fieldName
   * @returns {DOMElement}
   */
  findAddressSelectOption(selectEl, address, fieldName) {
    let value = address[fieldName];
    if (!value) {
      return null;
    }

    let country = address.country || this.DEFAULT_COUNTRY_CODE;
    let dataset = this.getCountryAddressData(country);
    let collators = this.getCollators(country);

    for (let option of selectEl.options) {
      if (this.strCompare(value, option.value, collators) ||
          this.strCompare(value, option.text, collators)) {
        return option;
      }
    }

    switch (fieldName) {
      case "address-level1": {
        if (!Array.isArray(dataset.sub_keys)) {
          dataset.sub_keys = dataset.sub_keys.split("~");
        }
        if (!Array.isArray(dataset.sub_names)) {
          dataset.sub_names = dataset.sub_names.split("~");
        }
        let keys = dataset.sub_keys;
        let names = dataset.sub_names;
        let identifiedValue = this.identifyValue(keys, names, value, collators);

        // No point going any further if we cannot identify value from address
        if (!identifiedValue) {
          return null;
        }

        // Go through options one by one to find a match.
        // Also check if any option contain the address-level1 key.
        let pattern = new RegExp("\\b" + this.escapeRegExp(identifiedValue) + "\\b", "i");
        for (let option of selectEl.options) {
          let optionValue = this.identifyValue(keys, names, option.value, collators);
          let optionText = this.identifyValue(keys, names, option.text, collators);
          if (identifiedValue === optionValue || identifiedValue === optionText || pattern.test(option.value)) {
            return option;
          }
        }
        break;
      }
      case "country": {
        if (ALTERNATIVE_COUNTRY_NAMES[value]) {
          for (let option of selectEl.options) {
            if (this.identifyCountryCode(option.text, value) || this.identifyCountryCode(option.value, value)) {
              return option;
            }
          }
        }
        break;
      }
    }

    return null;
  },

  findCreditCardSelectOption(selectEl, creditCard, fieldName) {
    let oneDigitMonth = creditCard["cc-exp-month"].toString();
    let twoDigitsMonth = oneDigitMonth.padStart(2, "0");
    let fourDigitsYear = creditCard["cc-exp-year"].toString();
    let twoDigitsYear = fourDigitsYear.substr(2, 2);
    let options = Array.from(selectEl.options);

    switch (fieldName) {
      case "cc-exp-month": {
        for (let option of options) {
          if ([option.text, option.label, option.value].some(s => {
            let result = /[1-9]\d*/.exec(s);
            return result && result[0] == oneDigitMonth;
          })) {
            return option;
          }
        }
        break;
      }
      case "cc-exp-year": {
        for (let option of options) {
          if ([option.text, option.label, option.value].some(
            s => s == twoDigitsYear || s == fourDigitsYear
          )) {
            return option;
          }
        }
        break;
      }
      case "cc-exp": {
        let patterns = [
          oneDigitMonth + "/" + twoDigitsYear,    // 8/22
          oneDigitMonth + "/" + fourDigitsYear,   // 8/2022
          twoDigitsMonth + "/" + twoDigitsYear,   // 08/22
          twoDigitsMonth + "/" + fourDigitsYear,  // 08/2022
          oneDigitMonth + "-" + twoDigitsYear,    // 8-22
          oneDigitMonth + "-" + fourDigitsYear,   // 8-2022
          twoDigitsMonth + "-" + twoDigitsYear,   // 08-22
          twoDigitsMonth + "-" + fourDigitsYear,  // 08-2022
          twoDigitsYear + "-" + twoDigitsMonth,   // 22-08
          fourDigitsYear + "-" + twoDigitsMonth,  // 2022-08
          fourDigitsYear + "/" + oneDigitMonth,   // 2022/8
          twoDigitsMonth + twoDigitsYear,         // 0822
          twoDigitsYear + twoDigitsMonth,         // 2208
        ];

        for (let option of options) {
          if ([option.text, option.label, option.value].some(
            str => patterns.some(pattern => str.includes(pattern))
          )) {
            return option;
          }
        }
        break;
      }
    }

    return null;
  },

  /**
   * Try to match value with keys and names, but always return the key.
   * @param   {array<string>} keys
   * @param   {array<string>} names
   * @param   {string} value
   * @param   {array} collators
   * @returns {string}
   */
  identifyValue(keys, names, value, collators) {
    let resultKey = keys.find(key => this.strCompare(value, key, collators));
    if (resultKey) {
      return resultKey;
    }

    let index = names.findIndex(name => this.strCompare(value, name, collators));
    if (index !== -1) {
      return keys[index];
    }

    return null;
  },

  /**
   * Compare if two strings are the same.
   * @param   {string} a
   * @param   {string} b
   * @param   {array} collators
   * @returns {boolean}
   */
  strCompare(a = "", b = "", collators) {
    return collators.some(collator => !collator.compare(a, b));
  },

  /**
   * Escaping user input to be treated as a literal string within a regular
   * expression.
   * @param   {string} string
   * @returns {string}
   */
  escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  },

  /**
   * Get formatting information of a given country
   * @param   {string} country
   * @returns {object}
   *         {
   *           {string} addressLevel1Label
   *           {string} postalCodeLabel
   *         }
   */
  getFormFormat(country) {
    const dataset = this.getCountryAddressData(country);
    return {
      "addressLevel1Label": dataset.state_name_type || "province",
      "postalCodeLabel": dataset.zip_name_type || "postalCode",
    };
  },

  /**
   * Localize elements with "data-localization" attribute
   * @param   {string} bundleURI
   * @param   {DOMElement} root
   */
  localizeMarkup(bundleURI, root) {
    const bundle = Services.strings.createBundle(bundleURI);
    let elements = root.querySelectorAll("[data-localization]");
    for (let element of elements) {
      element.textContent = bundle.GetStringFromName(element.getAttribute("data-localization"));
      element.removeAttribute("data-localization");
    }
  },
};

XPCOMUtils.defineLazyGetter(this.FormAutofillUtils, "DEFAULT_COUNTRY_CODE", () => {
  return Services.prefs.getCharPref("browser.search.countryCode", "US");
});

this.log = null;
this.FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

XPCOMUtils.defineLazyGetter(FormAutofillUtils, "stringBundle", function() {
  return Services.strings.createBundle("chrome://formautofill/locale/formautofill.properties");
});

XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "isAutofillAddressesEnabled", ENABLED_AUTOFILL_ADDRESSES_PREF);
XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "isAutofillCreditCardsEnabled", ENABLED_AUTOFILL_CREDITCARDS_PREF);
