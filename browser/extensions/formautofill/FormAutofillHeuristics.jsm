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
 * detail with FormAutofillHeuristics.getInfo function. It also provides a
 * cursor (parsingIndex) to indicate which element is waiting for parsing.
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
    this._parsingIndex = 0;
  }

  get _elements() {
    return this._elementsWeakRef.get();
  }

  /**
   * This cursor means the index of the element which is waiting for parsing.
   *
   * @returns {number}
   *          The index of the element which is waiting for parsing.
   */
  get parsingIndex() {
    return this._parsingIndex;
  }

  /**
   * Move the parsingIndex to the next elements. Any elements behind this index
   * means the parsing tasks are finished.
   *
   * @param {number} index
   *        The latest index of elements waiting for parsing.
   */
  set parsingIndex(index) {
    if (index > this.fieldDetails.length) {
      throw new Error("The parsing index is out of range.");
    }
    this._parsingIndex = index;
  }

  /**
   * Retrieve the field detail by the index. If the field detail is not ready,
   * the elements will be traversed until matching the index.
   *
   * @param {number} index
   *        The index of the element that you want to retrieve.
   * @returns {Object}
   *          The field detail at the specific index.
   */
  getFieldDetailByIndex(index) {
    if (index >= this._elements.length) {
      throw new Error(`The index ${index} is out of range.(${this._elements.length})`);
    }

    if (index < this.fieldDetails.length) {
      return this.fieldDetails[index];
    }

    for (let i = this.fieldDetails.length; i < (index + 1); i++) {
      this.pushDetail();
    }

    return this.fieldDetails[index];
  }

  get parsingFinished() {
    return this.parsingIndex >= this._elements.length;
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

  /**
   * When a field detail should be changed its fieldName after parsing, use
   * this function to update the fieldName which is at a specific index.
   *
   * @param {number} index
   *        The index indicates a field detail to be updated.
   * @param {string} fieldName
   *        The new fieldName
   */
  updateFieldName(index, fieldName) {
    if (index >= this.fieldDetails.length) {
      throw new Error("Try to update the non-existing field detail.");
    }
    this.fieldDetails[index].fieldName = fieldName;

    delete this.fieldDetails[index]._duplicated;
    let indexSame = this.findSameField(this.fieldDetails[index]);
    if (indexSame != index && indexSame != -1) {
      this.fieldDetails[index]._duplicated = true;
    }
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

  /**
   * Try to find the correct address-line[1-3] sequence and correct their field
   * names.
   *
   * @param {FieldScanner} fieldScanner
   *        The current parsing status for all elements
   * @returns {boolean}
   *          Return true if there is any field can be recognized in the parser,
   *          otherwise false.
   */
  _parseAddressFields(fieldScanner) {
    let parsedFields = false;
    let addressLines = ["address-line1", "address-line2", "address-line3"];
    for (let i = 0; !fieldScanner.parsingFinished && i < addressLines.length; i++) {
      let detail = fieldScanner.getFieldDetailByIndex(fieldScanner.parsingIndex);
      if (!detail || !addressLines.includes(detail.fieldName)) {
        // When the field is not related to any address-line[1-3] fields, it
        // means the parsing process can be terminated.
        break;
      }
      fieldScanner.updateFieldName(fieldScanner.parsingIndex, addressLines[i]);
      fieldScanner.parsingIndex++;
      parsedFields = true;
    }

    return parsedFields;
  },

  getFormInfo(form) {
    if (form.autocomplete == "off" || form.elements.length <= 0) {
      return [];
    }

    let fieldScanner = new FieldScanner(form.elements);
    while (!fieldScanner.parsingFinished) {
      let parsedAddressFields = this._parseAddressFields(fieldScanner);

      // If there is no any field parsed, the parsing cursor can be moved
      // forward to the next one.
      if (!parsedAddressFields) {
        fieldScanner.parsingIndex++;
      }
    }
    return fieldScanner.trimmedFieldDetail;
  },

  /**
   * Get the autocomplete info (e.g. fieldName) determined by the regexp
   * (this.RULES) matching to a feature string.
   *
   * @param {string} string a feature string to be determined.
   * @returns {Object}
   *          Provide the predicting result including the field name.
   *
   */
  _matchStringToFieldName(string) {
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

  getInfo(element) {
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

    for (let elementString of [element.id, element.name]) {
      let fieldNameResult = this._matchStringToFieldName(elementString);
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
        let fieldNameResult = this._matchStringToFieldName(string);
        if (fieldNameResult) {
          return fieldNameResult;
        }
      }
    }

    return null;
  },

/**
 * Phone field grammars - first matched grammar will be parsed. Grammars are
 * separated by { REGEX_SEPARATOR, FIELD_NONE, 0 }. Suffix and extension are
 * parsed separately unless they are necessary parts of the match.
 * The following notation is used to describe the patterns:
 * <cc> - country code field.
 * <ac> - area code field.
 * <phone> - phone or prefix.
 * <suffix> - suffix.
 * <ext> - extension.
 * :N means field is limited to N characters, otherwise it is unlimited.
 * (pattern <field>)? means pattern is optional and matched separately.
 *
 * This grammar list from Chromium will be enabled partially once we need to
 * support more cases of Telephone fields.
 */
  PHONE_FIELD_GRAMMARS: [
    // Country code: <cc> Area Code: <ac> Phone: <phone> (- <suffix>

    // (Ext: <ext>)?)?
      // {REGEX_COUNTRY, FIELD_COUNTRY_CODE, 0},
      // {REGEX_AREA, FIELD_AREA_CODE, 0},
      // {REGEX_PHONE, FIELD_PHONE, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // \( <ac> \) <phone>:3 <suffix>:4 (Ext: <ext>)?
      // {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 3},
      // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
      // {REGEX_PHONE, FIELD_SUFFIX, 4},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc> <ac>:3 - <phone>:3 - <suffix>:4 (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
      // {REGEX_PHONE, FIELD_AREA_CODE, 3},
      // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
      // {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc>:3 <ac>:3 <phone>:3 <suffix>:4 (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_COUNTRY_CODE, 3},
      // {REGEX_PHONE, FIELD_AREA_CODE, 3},
      // {REGEX_PHONE, FIELD_PHONE, 3},
      // {REGEX_PHONE, FIELD_SUFFIX, 4},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Area Code: <ac> Phone: <phone> (- <suffix> (Ext: <ext>)?)?
      // {REGEX_AREA, FIELD_AREA_CODE, 0},
      // {REGEX_PHONE, FIELD_PHONE, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> <phone>:3 <suffix>:4 (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_AREA_CODE, 0},
      // {REGEX_PHONE, FIELD_PHONE, 3},
      // {REGEX_PHONE, FIELD_SUFFIX, 4},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc> \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
      // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
      // {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0},
      // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: \( <ac> \) <phone> (- <suffix> (Ext: <ext>)?)?
      // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
      // {REGEX_AREA_NOTEXT, FIELD_AREA_CODE, 0},
      // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc> - <ac> - <phone> - <suffix> (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
      // {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0},
      // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 0},
      // {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Area code: <ac>:3 Prefix: <prefix>:3 Suffix: <suffix>:4 (Ext: <ext>)?
      // {REGEX_AREA, FIELD_AREA_CODE, 3},
      // {REGEX_PREFIX, FIELD_PHONE, 3},
      // {REGEX_SUFFIX, FIELD_SUFFIX, 4},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> Prefix: <phone> Suffix: <suffix> (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_AREA_CODE, 0},
      // {REGEX_PREFIX, FIELD_PHONE, 0},
      // {REGEX_SUFFIX, FIELD_SUFFIX, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> - <phone>:3 - <suffix>:4 (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_AREA_CODE, 0},
      // {REGEX_PREFIX_SEPARATOR, FIELD_PHONE, 3},
      // {REGEX_SUFFIX_SEPARATOR, FIELD_SUFFIX, 4},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc> - <ac> - <phone> (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_COUNTRY_CODE, 0},
      // {REGEX_PREFIX_SEPARATOR, FIELD_AREA_CODE, 0},
      // {REGEX_SUFFIX_SEPARATOR, FIELD_PHONE, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <ac> - <phone> (Ext: <ext>)?
      // {REGEX_AREA, FIELD_AREA_CODE, 0},
      // {REGEX_PHONE, FIELD_PHONE, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <cc>:3 - <phone>:10 (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_COUNTRY_CODE, 3},
      // {REGEX_PHONE, FIELD_PHONE, 10},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Ext: <ext>
      // {REGEX_EXTENSION, FIELD_EXTENSION, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},

    // Phone: <phone> (Ext: <ext>)?
      // {REGEX_PHONE, FIELD_PHONE, 0},
      // {REGEX_SEPARATOR, FIELD_NONE, 0},
  ],
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

