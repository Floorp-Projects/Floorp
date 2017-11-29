/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillUtils", "AddressDataLoader"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const ADDRESS_METADATA_PATH = "resource://formautofill/addressmetadata/";
const ADDRESS_REFERENCES = "addressReferences.js";
const ADDRESS_REFERENCES_EXT = "addressReferencesExt.js";

const ADDRESSES_COLLECTION_NAME = "addresses";
const CREDITCARDS_COLLECTION_NAME = "creditCards";
const ADDRESSES_FIRST_TIME_USE_PREF = "extensions.formautofill.firstTimeUse";
const ENABLED_AUTOFILL_ADDRESSES_PREF = "extensions.formautofill.addresses.enabled";
const CREDITCARDS_USED_STATUS_PREF = "extensions.formautofill.creditCards.used";
const AUTOFILL_CREDITCARDS_AVAILABLE_PREF = "extensions.formautofill.creditCards.available";
const ENABLED_AUTOFILL_CREDITCARDS_PREF = "extensions.formautofill.creditCards.enabled";
const SUPPORTED_COUNTRIES_PREF = "extensions.formautofill.supportedCountries";
const MANAGE_ADDRESSES_KEYWORDS = ["manageAddressesTitle", "addNewAddressTitle"];
const EDIT_ADDRESS_KEYWORDS = [
  "givenName", "additionalName", "familyName", "organization2", "streetAddress",
  "state", "province", "city", "country", "zip", "postalCode", "email", "tel",
];
const MANAGE_CREDITCARDS_KEYWORDS = ["manageCreditCardsTitle", "addNewCreditCardTitle", "showCreditCardsBtnLabel"];
const EDIT_CREDITCARD_KEYWORDS = ["cardNumber", "nameOnCard", "cardExpires"];
const FIELD_STATES = {
  NORMAL: "NORMAL",
  AUTO_FILLED: "AUTO_FILLED",
  PREVIEW: "PREVIEW",
};

// The maximum length of data to be saved in a single field for preventing DoS
// attacks that fill the user's hard drive(s).
const MAX_FIELD_VALUE_LENGTH = 200;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let AddressDataLoader = {
  // Status of address data loading. We'll load all the countries with basic level 1
  // information while requesting conutry information, and set country to true.
  // Level 1 Set is for recording which country's level 1/level 2 data is loaded,
  // since we only load this when getCountryAddressData called with level 1 parameter.
  _dataLoaded: {
    country: false,
    level1: new Set(),
  },
  /**
   * Load address data and extension script into a sandbox from different paths.
   * @param   {string} path
   *          The path for address data and extension script. It could be root of the address
   *          metadata folder(addressmetadata/) or under specific country(addressmetadata/TW/).
   * @returns {object}
   *          A sandbox that contains address data object with properties from extension.
   */
  _loadScripts(path) {
    let sandbox = {};
    let extSandbox = {};

    try {
      sandbox = FormAutofillUtils.loadDataFromScript(path + ADDRESS_REFERENCES);
      extSandbox = FormAutofillUtils.loadDataFromScript(path + ADDRESS_REFERENCES_EXT);
    } catch (e) {
      // Will return only address references if extension loading failed or empty sandbox if
      // address references loading failed.
      return sandbox;
    }

    if (extSandbox.addressDataExt) {
      for (let key in extSandbox.addressDataExt) {
        Object.assign(sandbox.addressData[key], extSandbox.addressDataExt[key]);
      }
    }
    return sandbox;
  },
  /**
   * We'll cache addressData in the loader once the data loaded from scripts.
   * It'll become the example below after loading addressReferences with extension:
   * addressData: {
                   "data/US": {"lang": "en", ...// Data defined in libaddressinput metadata
   *                           "alternative_names": ... // Data defined in extension }
   *               "data/CA": {} // Other supported country metadata
   *               "data/TW": {} // Other supported country metadata
   *               "data/TW/台北市": {} // Other supported country level 1 metadata
   *              }
   * @param   {string} country
   * @param   {string} level1
   * @returns {object}
   */
  getData(country, level1 = null) {
    // Load the addressData if needed
    if (!this._dataLoaded.country) {
      this._addressData = this._loadScripts(ADDRESS_METADATA_PATH).addressData;
      this._dataLoaded.country = true;
    }
    if (!level1) {
      return this._addressData[`data/${country}`];
    }
    // If level1 is set, load addressReferences under country folder with specific
    // country/level 1 for level 2 information.
    if (!this._dataLoaded.level1.has(country)) {
      Object.assign(this._addressData,
                    this._loadScripts(`${ADDRESS_METADATA_PATH}${country}/`).addressData);
      this._dataLoaded.level1.add(country);
    }
    return this._addressData[`data/${country}/${level1}`];
  },
};

this.FormAutofillUtils = {
  get AUTOFILL_FIELDS_THRESHOLD() { return 3; },
  get isAutofillEnabled() { return this.isAutofillAddressesEnabled || this.isAutofillCreditCardsEnabled; },
  get isAutofillCreditCardsEnabled() { return this.isAutofillCreditCardsAvailable && this._isAutofillCreditCardsEnabled; },

  ADDRESSES_COLLECTION_NAME,
  CREDITCARDS_COLLECTION_NAME,
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
  ADDRESSES_FIRST_TIME_USE_PREF,
  CREDITCARDS_USED_STATUS_PREF,
  MANAGE_ADDRESSES_KEYWORDS,
  EDIT_ADDRESS_KEYWORDS,
  MANAGE_CREDITCARDS_KEYWORDS,
  EDIT_CREDITCARD_KEYWORDS,
  MAX_FIELD_VALUE_LENGTH,
  FIELD_STATES,

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

  _collators: {},
  _reAlternativeCountryNames: {},

  isAddressField(fieldName) {
    return !!this._fieldNameInfo[fieldName] && !this.isCreditCardField(fieldName);
  },

  isCreditCardField(fieldName) {
    return this._fieldNameInfo[fieldName] == "creditCard";
  },

  normalizeCCNumber(ccNumber) {
    ccNumber = ccNumber.replace(/[-\s]/g, "");

    // Based on the information on wiki[1], the shortest valid length should be
    // 12 digits(Maestro).
    // [1] https://en.wikipedia.org/wiki/Payment_card_number
    return ccNumber.match(/^\d{12,}$/) ? ccNumber : null;
  },

  isCCNumber(ccNumber) {
    return !!this.normalizeCCNumber(ccNumber);
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

  /**
   * In-place concatenate tel-related components into a single "tel" field and
   * delete unnecessary fields.
   * @param {object} address An address record.
   */
  compressTel(address) {
    let telCountryCode = address["tel-country-code"] || "";
    let telAreaCode = address["tel-area-code"] || "";

    if (!address.tel) {
      if (address["tel-national"]) {
        address.tel = telCountryCode + address["tel-national"];
      } else if (address["tel-local"]) {
        address.tel = telCountryCode + telAreaCode + address["tel-local"];
      } else if (address["tel-local-prefix"] && address["tel-local-suffix"]) {
        address.tel = telCountryCode + telAreaCode + address["tel-local-prefix"] + address["tel-local-suffix"];
      }
    }

    for (let field in address) {
      if (field != "tel" && this.getCategoryFromFieldName(field) == "tel") {
        delete address[field];
      }
    }
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

  ALLOWED_TYPES: ["text", "email", "tel", "number", "month"],
  isFieldEligibleForAutofill(element) {
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
    Services.scriptloader.loadSubScript(url, sandbox, "utf-8");
    return sandbox;
  },

  // Get country address data and fallback to US if not found.
  // See AddressDataLoader.getData for more details of addressData structure.
  getCountryAddressData(country, level1 = null) {
    let metadata = AddressDataLoader.getData(country, level1);
    if (!metadata) {
      metadata = level1 ? null : AddressDataLoader.getData("US");
    }

    return metadata;
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
   * Parse a country address format string and outputs an array of fields.
   * Spaces, commas, and other literals are ignored in this implementation.
   * For example, format string "%A%n%C, %S" should return:
   * [
   *   {fieldId: "street-address", newLine: true},
   *   {fieldId: "address-level2"},
   *   {fieldId: "address-level1"},
   * ]
   *
   * @param   {string} fmt Country address format string
   * @returns {array<object>} List of fields
   */
  parseAddressFormat(fmt) {
    if (!fmt) {
      throw new Error("fmt string is missing.");
    }
    // Based on the list of fields abbreviations in
    // https://github.com/googlei18n/libaddressinput/wiki/AddressValidationMetadata
    const fieldsLookup = {
      N: "name",
      O: "organization",
      A: "street-address",
      S: "address-level1",
      C: "address-level2",
      Z: "postal-code",
      n: "newLine",
    };

    return fmt.match(/%[^%]/g).reduce((parsed, part) => {
      // Take the first letter of each segment and try to identify it
      let fieldId = fieldsLookup[part[1]];
      // Early return if cannot identify part.
      if (!fieldId) {
        return parsed;
      }
      // If a new line is detected, add an attribute to the previous field.
      if (fieldId == "newLine") {
        let size = parsed.length;
        if (size) {
          parsed[size - 1].newLine = true;
        }
        return parsed;
      }
      return parsed.concat({fieldId});
    }, []);
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
    let countries = countrySpecified ? [countrySpecified] : this.supportedCountries;

    for (let country of countries) {
      let collators = this.getCollators(country);

      let metadata = this.getCountryAddressData(country);
      let alternativeCountryNames = metadata.alternative_names || [metadata.name];
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
        if (this.getCountryAddressData(value).alternative_names) {
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
    let oneDigitMonth = creditCard["cc-exp-month"] ? creditCard["cc-exp-month"].toString() : null;
    let twoDigitsMonth = oneDigitMonth ? oneDigitMonth.padStart(2, "0") : null;
    let fourDigitsYear = creditCard["cc-exp-year"] ? creditCard["cc-exp-year"].toString() : null;
    let twoDigitsYear = fourDigitsYear ? fourDigitsYear.substr(2, 2) : null;
    let options = Array.from(selectEl.options);

    switch (fieldName) {
      case "cc-exp-month": {
        if (!oneDigitMonth) {
          return null;
        }
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
        if (!fourDigitsYear) {
          return null;
        }
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
        if (!oneDigitMonth || !fourDigitsYear) {
          return null;
        }
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

XPCOMUtils.defineLazyGetter(FormAutofillUtils, "brandBundle", function() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});

XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "isAutofillAddressesEnabled", ENABLED_AUTOFILL_ADDRESSES_PREF);
XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "isAutofillCreditCardsAvailable", AUTOFILL_CREDITCARDS_AVAILABLE_PREF);
XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "_isAutofillCreditCardsEnabled", ENABLED_AUTOFILL_CREDITCARDS_PREF);
XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "isAutofillAddressesFirstTimeUse", ADDRESSES_FIRST_TIME_USE_PREF);
XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "AutofillCreditCardsUsedStatus", CREDITCARDS_USED_STATUS_PREF);
XPCOMUtils.defineLazyPreferenceGetter(this.FormAutofillUtils,
                                      "supportedCountries", SUPPORTED_COUNTRIES_PREF, null, null,
                                      val => val.split(","));
