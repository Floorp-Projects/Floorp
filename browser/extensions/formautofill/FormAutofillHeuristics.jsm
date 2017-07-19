/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill field heuristics.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillHeuristics"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const PREF_HEURISTICS_ENABLED = "extensions.formautofill.heuristics.enabled";

/**
 * A scanner for traversing all elements in a form and retrieving the field
 * detail with FormAutofillHeuristics.getInfo function.
 */
class FieldScanner {
  /**
   * Create a FieldScanner based on form elements with the existing
   * fieldDetails.
   *
   * @param {Array.DOMElement} elements
   *        The elements from a form for each parser.
   */
  constructor(elements) {
    this._elementsWeakRef = Cu.getWeakReference(elements);
    this.fieldDetails = [];
  }

  get _elements() {
    return this._elementsWeakRef.get();
  }

  /**
   * This function will prepare an autocomplete info object with getInfo
   * function and push the detail to fieldDetails property. Any duplicated
   * detail will be marked as _duplicated = true for the parser.
   *
   * Any element without the related detail will be used for adding the detail
   * to the end of field details.
   */
  pushDetail() {
    let elementIndex = this.fieldDetails.length;
    if (elementIndex >= this._elements.length) {
      throw new Error("Try to push the non-existing element info.");
    }
    let element = this._elements[elementIndex];
    let info = FormAutofillHeuristics.getInfo(element);
    if (!info) {
      info = {};
    }
    let fieldInfo = {
      section: info.section,
      addressType: info.addressType,
      contactType: info.contactType,
      fieldName: info.fieldName,
      elementWeakRef: Cu.getWeakReference(element),
    };

    // Store the association between the field metadata and the element.
    if (this.findSameField(info) != -1) {
      // A field with the same identifier already exists.
      log.debug("Not collecting a field matching another with the same info:", info);
      fieldInfo._duplicated = true;
    }

    this.fieldDetails.push(fieldInfo);
  }

  findSameField(info) {
    return this.fieldDetails.findIndex(f => f.section == info.section &&
                                       f.addressType == info.addressType &&
                                       f.contactType == info.contactType &&
                                       f.fieldName == info.fieldName);
  }

  /**
   * Provide the field details without invalid field name and duplicated fields.
   *
   * @returns {Array<Object>}
   *          The array with the field details without invalid field name and
   *          duplicated fields.
   */
  get trimmedFieldDetail() {
    return this.fieldDetails.filter(f => f.fieldName && !f._duplicated);
  }
}

/**
 * Returns the autocomplete information of fields according to heuristics.
 */
this.FormAutofillHeuristics = {
  FIELD_GROUPS: {
    NAME: [
      "name",
      "given-name",
      "additional-name",
      "family-name",
    ],
    ADDRESS: [
      "organization",
      "street-address",
      "address-line1",
      "address-line2",
      "address-line3",
      "address-level2",
      "address-level1",
      "postal-code",
      "country",
    ],
    TEL: ["tel"],
    EMAIL: ["email"],
  },

  RULES: null,

  getFormInfo(form) {
    if (form.autocomplete == "off") {
      return [];
    }
    let fieldScanner = new FieldScanner(form.elements);
    for (let i = 0; i < fieldScanner.elements.length; i++) {
      let element = fieldScanner.elements[i];
      let info = this.getInfo(element, fieldScanner.fieldDetails);
      fieldScanner.pushDetail(i, info);
    }
    return fieldScanner.trimmedFieldDetail;
  },

  /**
   * Get the autocomplete info (e.g. fieldName) determined by the regexp
   * (this.RULES) matching to a feature string. The result is based on the
   * existing field names to prevent duplicating predictions
   * (e.g. address-line[1-3).
   *
   * @param {string} string a feature string to be determined.
   * @param {Array<string>} existingFieldNames the array of exising field names
   *                        in a form.
   * @returns {Object}
   *          Provide the predicting result including the field name.
   *
   */
  _matchStringToFieldName(string, existingFieldNames) {
    let result = {
      fieldName: "",
      section: "",
      addressType: "",
      contactType: "",
    };
    if (this.RULES.email.test(string)) {
      result.fieldName = "email";
      return result;
    }
    if (this.RULES.tel.test(string)) {
      result.fieldName = "tel";
      return result;
    }
    for (let fieldName of this.FIELD_GROUPS.ADDRESS) {
      if (this.RULES[fieldName].test(string)) {
        // If "address-line1" or "address-line2" exist already, the string
        // could be satisfied with "address-line2" or "address-line3".
        if ((fieldName == "address-line1" &&
            existingFieldNames.includes("address-line1")) ||
            (fieldName == "address-line2" &&
            existingFieldNames.includes("address-line2"))) {
          continue;
        }
        result.fieldName = fieldName;
        return result;
      }
    }
    for (let fieldName of this.FIELD_GROUPS.NAME) {
      if (this.RULES[fieldName].test(string)) {
        result.fieldName = fieldName;
        return result;
      }
    }
    return null;
  },

  getInfo(element, fieldDetails) {
    if (!FormAutofillUtils.isFieldEligibleForAutofill(element)) {
      return null;
    }

    let info = element.getAutocompleteInfo();
    // An input[autocomplete="on"] will not be early return here since it stll
    // needs to find the field name.
    if (info && info.fieldName && info.fieldName != "on") {
      return info;
    }

    if (!this._prefEnabled) {
      return null;
    }

    // "email" type of input is accurate for heuristics to determine its Email
    // field or not. However, "tel" type is used for ZIP code for some web site
    // (e.g. HomeDepot, BestBuy), so "tel" type should be not used for "tel"
    // prediction.
    if (element.type == "email") {
      return {
        fieldName: "email",
        section: "",
        addressType: "",
        contactType: "",
      };
    }

    let existingFieldNames = fieldDetails ? fieldDetails.map(i => i.fieldName) : "";

    for (let elementString of [element.id, element.name]) {
      let fieldNameResult = this._matchStringToFieldName(elementString,
                                                         existingFieldNames);
      if (fieldNameResult) {
        return fieldNameResult;
      }
    }
    let labels = FormAutofillUtils.findLabelElements(element);
    if (!labels || labels.length == 0) {
      log.debug("No label found for", element);
      return null;
    }
    for (let label of labels) {
      let strings = FormAutofillUtils.extractLabelStrings(label);
      for (let string of strings) {
        let fieldNameResult = this._matchStringToFieldName(string,
                                                           existingFieldNames);
        if (fieldNameResult) {
          return fieldNameResult;
        }
      }
    }

    return null;
  },
};

XPCOMUtils.defineLazyGetter(this.FormAutofillHeuristics, "RULES", () => {
  let sandbox = {};
  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                       .getService(Ci.mozIJSSubScriptLoader);
  const HEURISTICS_REGEXP = "chrome://formautofill/content/heuristicsRegexp.js";
  scriptLoader.loadSubScript(HEURISTICS_REGEXP, sandbox, "utf-8");
  return sandbox.HeuristicsRegExp.RULES;
});

XPCOMUtils.defineLazyGetter(this.FormAutofillHeuristics, "_prefEnabled", () => {
  return Services.prefs.getBoolPref(PREF_HEURISTICS_ENABLED);
});

Services.prefs.addObserver(PREF_HEURISTICS_ENABLED, () => {
  this.FormAutofillHeuristics._prefEnabled = Services.prefs.getBoolPref(PREF_HEURISTICS_ENABLED);
});

