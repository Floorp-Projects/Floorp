/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const ADDRESS_REFERENCES = "chrome://formautofill/content/addressReferences.js";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.FormAutofillUtils = {
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
    "email": "email",
    "cc-name": "creditCard",
    "cc-number": "creditCard",
    "cc-exp-month": "creditCard",
    "cc-exp-year": "creditCard",
  },
  _addressDataLoaded: false,

  isAddressField(fieldName) {
    return !!this._fieldNameInfo[fieldName] && !this.isCreditCardField(fieldName);
  },

  isCreditCardField(fieldName) {
    return this._fieldNameInfo[fieldName] == "creditCard";
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

    if (element instanceof Ci.nsIDOMHTMLInputElement) {
      // `element.type` can be recognized as `text`, if it's missing or invalid.
      if (!this.ALLOWED_TYPES.includes(element.type)) {
        return false;
      }
    } else if (!(element instanceof Ci.nsIDOMHTMLSelectElement)) {
      return false;
    }

    return true;
  },

  // The tag name list is from Chromium except for "STYLE":
  // eslint-disable-next-line max-len
  // https://cs.chromium.org/chromium/src/components/autofill/content/renderer/form_autofill_util.cc?l=216&rcl=d33a171b7c308a64dc3372fac3da2179c63b419e
  EXCLUDED_TAGS: ["SCRIPT", "NOSCRIPT", "OPTION", "STYLE"],
  /**
   * Extract all strings of an element's children to an array.
   * "element.textContent" is a string which is merged of all children nodes,
   * and this function provides an array of the strings contains in an element.
   *
   * @param  {Object} element
   *         A DOM element to be extracted.
   * @returns {Array}
   *          All strings in an element.
   */
  extractLabelStrings(element) {
    let strings = [];
    let _extractLabelStrings = (el) => {
      if (this.EXCLUDED_TAGS.includes(el.tagName)) {
        return;
      }

      if (el.nodeType == Ci.nsIDOMNode.TEXT_NODE ||
          el.childNodes.length == 0) {
        let trimmedText = el.textContent.trim();
        if (trimmedText) {
          strings.push(trimmedText);
        }
        return;
      }

      for (let node of el.childNodes) {
        if (node.nodeType != Ci.nsIDOMNode.ELEMENT_NODE &&
            node.nodeType != Ci.nsIDOMNode.TEXT_NODE) {
          continue;
        }
        _extractLabelStrings(node);
      }
    };
    _extractLabelStrings(element);
    return strings;
  },

  findLabelElements(element) {
    let document = element.ownerDocument;
    let labels = [];
    // TODO: querySelectorAll is inefficient here. However, bug 1339726 is for
    // a more efficient implementation from DOM API perspective. This function
    // should be refined after input.labels API landed.
    for (let label of document.querySelectorAll("label[for]")) {
      if (element.id == label.htmlFor) {
        labels.push(label);
      }
    }

    if (labels.length > 0) {
      log.debug("Label found by ID", element.id);
      return labels;
    }

    let parent = element.parentNode;
    if (!parent) {
      return [];
    }
    do {
      if (parent.tagName == "LABEL" &&
          parent.control == element &&
          !parent.hasAttribute("for")) {
        log.debug("Label found in input's parent or ancestor.");
        return [parent];
      }
      parent = parent.parentNode;
    } while (parent);

    return [];
  },

  loadDataFromScript(url, sandbox = {}) {
    let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                         .getService(Ci.mozIJSSubScriptLoader);
    scriptLoader.loadSubScript(url, sandbox, "utf-8");
    return sandbox;
  },

  /**
   * Find the option element from select element.
   * 1. Try to find the locale using the country from profile.
   * 2. First pass try to find exact match.
   * 3. Second pass try to identify values from profile value and options,
   *    and look for a match.
   * @param   {DOMElement} selectEl
   * @param   {object} profile
   * @param   {string} fieldName
   * @returns {DOMElement}
   */
  findSelectOption(selectEl, profile, fieldName) {
    let value = profile[fieldName];
    if (!value) {
      return null;
    }

    // Load the addressData if needed
    if (!this._addressDataLoaded) {
      Object.assign(this, this.loadDataFromScript(ADDRESS_REFERENCES));
      this._addressDataLoaded = true;
    }

    // Set dataset to "data/US" as fallback
    let dataset = this.addressData[`data/${profile.country}`] ||
                  this.addressData["data/US"];
    let collator = new Intl.Collator(dataset.lang, {sensitivity: "base", ignorePunctuation: true});

    for (let option of selectEl.options) {
      if (this.strCompare(value, option.value, collator) ||
          this.strCompare(value, option.text, collator)) {
        return option;
      }
    }

    if (fieldName === "address-level1") {
      if (!Array.isArray(dataset.sub_keys)) {
        dataset.sub_keys = dataset.sub_keys.split("~");
      }
      if (!Array.isArray(dataset.sub_names)) {
        dataset.sub_names = dataset.sub_names.split("~");
      }
      let keys = dataset.sub_keys;
      let names = dataset.sub_names;
      let identifiedValue = this.identifyValue(keys, names, value, collator);

      // No point going any further if we cannot identify value from profile
      if (identifiedValue === undefined) {
        return null;
      }

      // Go through options one by one to find a match.
      // Also check if any option contain the address-level1 key.
      let pattern = new RegExp(`\\b${identifiedValue}\\b`, "i");
      for (let option of selectEl.options) {
        let optionValue = this.identifyValue(keys, names, option.value, collator);
        let optionText = this.identifyValue(keys, names, option.text, collator);
        if (identifiedValue === optionValue || identifiedValue === optionText || pattern.test(option.value)) {
          return option;
        }
      }
    }

    if (fieldName === "country") {
      // TODO: Support matching countries (Bug 1375382)
    }

    return null;
  },

  /**
   * Try to match value with keys and names, but always return the key.
   * @param   {array<string>} keys
   * @param   {array<string>} names
   * @param   {string} value
   * @param   {object} collator
   * @returns {string}
   */
  identifyValue(keys, names, value, collator) {
    let resultKey = keys.find(key => this.strCompare(value, key, collator));
    if (resultKey) {
      return resultKey;
    }

    let index = names.findIndex(name => this.strCompare(value, name, collator));
    if (index !== -1) {
      return keys[index];
    }

    return null;
  },

  /**
   * Compare if two strings are the same.
   * @param   {string} a
   * @param   {string} b
   * @param   {object} collator
   * @returns {boolean}
   */
  strCompare(a = "", b = "", collator) {
    return !collator.compare(a, b);
  },
};

this.log = null;
this.FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);
