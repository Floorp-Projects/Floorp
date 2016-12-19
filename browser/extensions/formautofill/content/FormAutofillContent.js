/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill frame script.
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr, manager: Cm} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
let {FormAutoCompleteResult} = Cu.import("resource://gre/modules/nsFormAutoCompleteResult.jsm", {});

/**
 * Handles profile autofill for a DOM Form element.
 * @param {HTMLFormElement} form Form that need to be auto filled
 */
function FormAutofillHandler(form) {
  this.form = form;
  this.fieldDetails = [];
}

FormAutofillHandler.prototype = {
  /**
   * DOM Form element to which this object is attached.
   */
  form: null,

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
   * Returns information from the form about fields that can be autofilled, and
   * populates the fieldDetails array on this object accordingly.
   *
   * @returns {Array<Object>} Serializable data structure that can be sent to the user
   *          interface, or null if the operation failed because the constraints
   *          on the allowed fields were not honored.
   */
  collectFormFields() {
    let autofillData = [];

    for (let element of this.form.elements) {
      // Query the interface and exclude elements that cannot be autocompleted.
      if (!(element instanceof Ci.nsIDOMHTMLInputElement)) {
        continue;
      }

      // Exclude elements to which no autocomplete field has been assigned.
      let info = element.getAutocompleteInfo();
      if (!info.fieldName || ["on", "off"].includes(info.fieldName)) {
        continue;
      }

      // Store the association between the field metadata and the element.
      if (this.fieldDetails.some(f => f.section == info.section &&
                                      f.addressType == info.addressType &&
                                      f.contactType == info.contactType &&
                                      f.fieldName == info.fieldName)) {
        // A field with the same identifier already exists.
        return null;
      }

      let inputFormat = {
        section: info.section,
        addressType: info.addressType,
        contactType: info.contactType,
        fieldName: info.fieldName,
      };
      // Clone the inputFormat for caching the fields and elements together
      let formatWithElement = Object.assign({}, inputFormat);

      inputFormat.index = autofillData.length;
      autofillData.push(inputFormat);

      formatWithElement.element = element;
      this.fieldDetails.push(formatWithElement);
    }

    return autofillData;
  },

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * data provided by backend.
   *
   * @param {Array<Object>} autofillResult
   *        Data returned by the user interface.
   *        [{
   *          section: Value originally provided to the user interface.
   *          addressType: Value originally provided to the user interface.
   *          contactType: Value originally provided to the user interface.
   *          fieldName: Value originally provided to the user interface.
   *          value: String with which the field should be updated.
   *          index: Index to match the input in fieldDetails
   *        }],
   *        }
   */
  autofillFormFields(autofillResult) {
    for (let field of autofillResult) {
      // Get the field details, if it was processed by the user interface.
      let fieldDetail = this.fieldDetails[field.index];

      // Avoid the invalid value set
      if (!fieldDetail || !field.value) {
        continue;
      }

      let info = fieldDetail.element.getAutocompleteInfo();
      if (field.section != info.section ||
          field.addressType != info.addressType ||
          field.contactType != info.contactType ||
          field.fieldName != info.fieldName) {
        Cu.reportError("Autocomplete tokens mismatched");
        continue;
      }

      fieldDetail.element.setUserInput(field.value);
    }
  },
};

// Register/unregister a constructor as a factory.
function AutocompleteFactory() {}
AutocompleteFactory.prototype = {
  register(targetConstructor) {
    let proto = targetConstructor.prototype;
    this._classID = proto.classID;

    let factory = XPCOMUtils._getFactory(targetConstructor);
    this._factory = factory;

    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(proto.classID, proto.classDescription,
                              proto.contractID, factory);

    if (proto.classID2) {
      this._classID2 = proto.classID2;
      registrar.registerFactory(proto.classID2, proto.classDescription,
                                proto.contractID2, factory);
    }
  },

  unregister() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._classID, this._factory);
    if (this._classID2) {
      registrar.unregisterFactory(this._classID2, this._factory);
    }
    this._factory = null;
  },
};


/**
 * @constructor
 *
 * @implements {nsIAutoCompleteSearch}
 */
function AutofillProfileAutoCompleteSearch() {

}
AutofillProfileAutoCompleteSearch.prototype = {
  classID: Components.ID("4f9f1e4c-7f2c-439e-9c9e-566b68bc187d"),
  contractID: "@mozilla.org/autocomplete/search;1?name=autofill-profiles",
  classDescription: "AutofillProfileAutoCompleteSearch",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteSearch]),

  // Begin nsIAutoCompleteSearch implementation

  /**
   * Searches for a given string and notifies a listener (either synchronously
   * or asynchronously) of the result
   *
   * @param {string} searchString the string to search for
   * @param {string} searchParam
   * @param {Object} previousResult a previous result to use for faster searchinig
   * @param {Object} listener the listener to notify when the search is complete
   */
  startSearch(searchString, searchParam, previousResult, listener) {
    // TODO: These mock data should be replaced by form autofill API
    let labels = ["Mary", "John"];
    let values = ["Mary S.", "John S."];
    let comments = ["123 Sesame Street.", "331 E. Evelyn Avenue"];
    let result = new FormAutoCompleteResult(searchString,
                                            Ci.nsIAutoCompleteResult.RESULT_SUCCESS,
                                            0, "", values, labels,
                                            comments);

    listener.onSearchResult(this, result);
  },

  /**
   * Stops an asynchronous search that is in progress
   */
  stopSearch() {
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AutofillProfileAutoCompleteSearch]);

// TODO: Remove this lint option once we apply ProfileAutocomplete while
//       content script initialization.
/* eslint no-unused-vars: [2, {"vars": "local"}] */
let ProfileAutocomplete = {
  _registered: false,
  _factory: null,

  ensureRegistered() {
    if (this._registered) {
      return;
    }

    this._factory = new AutocompleteFactory();
    this._factory.register(AutofillProfileAutoCompleteSearch);
    this._registered = true;
  },

  ensureUnregistered() {
    if (!this._registered) {
      return;
    }

    this._factory.unregister();
    this._factory = null;
    this._registered = false;
  },
};
