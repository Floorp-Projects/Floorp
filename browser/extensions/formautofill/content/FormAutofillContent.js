/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-use-before-define */

/*
 * Form Autofill frame script.
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr, manager: Cm} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ProfileAutoCompleteResult",
                                  "resource://formautofill/ProfileAutoCompleteResult.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormLikeFactory",
                                  "resource://gre/modules/FormLikeFactory.jsm");

const formFillController = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                             .getService(Ci.nsIFormFillController);

const AUTOFILL_FIELDS_THRESHOLD = 3;

/**
 * Returns the autocomplete information of fields according to heuristics.
 */
let FormAutofillHeuristics = {
  VALID_FIELDS: [
    "organization",
    "street-address",
    "address-level2",
    "address-level1",
    "postal-code",
    "country",
    "tel",
    "email",
  ],

  getInfo(element) {
    if (!(element instanceof Ci.nsIDOMHTMLInputElement)) {
      return null;
    }

    let info = element.getAutocompleteInfo();
    if (!info || !info.fieldName ||
        !this.VALID_FIELDS.includes(info.fieldName)) {
      return null;
    }

    return info;
  },
};

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
      // Exclude elements to which no autocomplete field has been assigned.
      let info = FormAutofillHeuristics.getInfo(element);
      if (!info) {
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

      let info = FormAutofillHeuristics.getInfo(fieldDetail.element);
      if (!info ||
          field.section != info.section ||
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
    if (proto.classID && !registrar.isCIDRegistered(proto.classID)) {
      registrar.registerFactory(proto.classID, proto.classDescription,
                                proto.contractID, factory);
    }

    if (proto.classID2 && !registrar.isCIDRegistered(proto.classID2)) {
      this._classID2 = proto.classID2;
      registrar.registerFactory(proto.classID2, proto.classDescription,
                                proto.contractID2, factory);
    }
  },

  unregister() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    if (this._classID && registrar.isCIDRegistered(this._classID)) {
      registrar.unregisterFactory(this._classID, this._factory);
    }

    if (this._classID2 && registrar.isCIDRegistered(this._classID2)) {
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
    this.forceStop = false;
    let info = this.getInputDetails();

    this.getProfiles({info, searchString}).then((profiles) => {
      if (this.forceStop) {
        return;
      }

      // TODO: Set formInfo for ProfileAutoCompleteResult
      // let formInfo = this.getFormDetails();
      let result = new ProfileAutoCompleteResult(searchString, info, profiles, {});

      listener.onSearchResult(this, result);
    });
  },

  /**
   * Stops an asynchronous search that is in progress
   */
  stopSearch() {
    this.forceStop = true;
  },

  /**
   * Get the profile data from parent process for AutoComplete result.
   *
   * @private
   * @param  {Object} data
   *         Parameters for querying the corresponding result.
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched profile.
   * @param  {string} data.info
   *         The input autocomplete property's information.
   * @returns {Promise}
   *          Promise that resolves when profiles returned from parent process.
   */
  getProfiles(data) {
    return new Promise((resolve) => {
      addMessageListener("FormAutofill:Profiles", function getResult(result) {
        removeMessageListener("FormAutofill:Profiles", getResult);
        resolve(result.data);
      });

      sendAsyncMessage("FormAutofill:GetProfiles", data);
    });
  },


  /**
   * Get the input's information from FormAutofillContent's cache.
   *
   * @returns {Object}
   *          Target input's information that cached in FormAutofillContent.
   */
  getInputDetails() {
    // TODO: Maybe we'll need to wait for cache ready if detail is empty.
    return FormAutofillContent.getInputDetails(formFillController.focusedInput);
  },

  /**
   * Get the form's information from FormAutofillContent's cache.
   *
   * @returns {Array<Object>}
   *          Array of the inputs' information for the target form.
   */
  getFormDetails() {
    // TODO: Maybe we'll need to wait for cache ready if details is empty.
    return FormAutofillContent.getFormDetails(formFillController.focusedInput);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AutofillProfileAutoCompleteSearch]);

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

/**
 * Handles content's interactions.
 *
 * NOTE: Declares it by "var" to make it accessible in unit tests.
 */
var FormAutofillContent = {
  init() {
    ProfileAutocomplete.ensureRegistered();

    addEventListener("DOMContentLoaded", this);
  },

  handleEvent(evt) {
    if (!evt.isTrusted) {
      return;
    }

    switch (evt.type) {
      case "DOMContentLoaded":
        let doc = evt.target;
        if (!(doc instanceof Ci.nsIDOMHTMLDocument)) {
          return;
        }
        this._identifyAutofillFields(doc);
        break;
    }
  },

  /**
   * Get the input's information from cache which is created after page identified.
   *
   * @param {HTMLInputElement} element Focused input which triggered profile searching
   * @returns {Object|null}
   *          Return target input's information that cloned from content cache
   *          (or return null if the information is not found in the cache).
   */
  getInputDetails(element) {
    for (let formDetails of this._formsDetails) {
      for (let detail of formDetails) {
        if (element == detail.element) {
          return this._serializeInfo(detail);
        }
      }
    }
    return null;
  },

  /**
   * Get the form's information from cache which is created after page identified.
   *
   * @param {HTMLInputElement} element Focused input which triggered profile searching
   * @returns {Array<Object>|null}
   *          Return target form's information that cloned from content cache
   *          (or return null if the information is not found in the cache).
   *
   */
  getFormDetails(element) {
    for (let formDetails of this._formsDetails) {
      if (formDetails.some((detail) => detail.element == element)) {
        return formDetails.map((detail) => this._serializeInfo(detail));
      }
    }
    return null;
  },

  /**
   * Create a clone the information object without element reference.
   *
   * @param {Object} detail Profile autofill information for specific input.
   * @returns {Object}
   *          Return a copy of cached information object without element reference
   *          since it's not needed for creating result.
   */
  _serializeInfo(detail) {
    let info = Object.assign({}, detail);
    delete info.element;
    return info;
  },

  _identifyAutofillFields(doc) {
    let forms = [];
    this._formsDetails = [];

    // Collects root forms from inputs.
    for (let field of doc.getElementsByTagName("input")) {
      // We only consider text-like fields for now until we support radio and
      // checkbox buttons in the future.
      if (!field.mozIsTextField(true)) {
        continue;
      }

      let formLike = FormLikeFactory.createFromField(field);
      if (!forms.some(form => form.rootElement === formLike.rootElement)) {
        forms.push(formLike);
      }
    }

    // Collects the fields that can be autofilled from each form and marks them
    // as autofill fields if the amount is above the threshold.
    forms.forEach(form => {
      let formHandler = new FormAutofillHandler(form);
      formHandler.collectFormFields();
      if (formHandler.fieldDetails.length < AUTOFILL_FIELDS_THRESHOLD) {
        return;
      }

      this._formsDetails.push(formHandler.fieldDetails);
      formHandler.fieldDetails.forEach(
        detail => this._markAsAutofillField(detail.element));
    });
  },

  _markAsAutofillField(field) {
    formFillController.markAsAutofillField(field);
  },
};


FormAutofillContent.init();
