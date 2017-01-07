/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill content process module.
 */

/* eslint-disable no-use-before-define */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillContent"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr, manager: Cm} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ProfileAutoCompleteResult",
                                  "resource://formautofill/ProfileAutoCompleteResult.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillHandler",
                                  "resource://formautofill/FormAutofillHandler.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormLikeFactory",
                                  "resource://gre/modules/FormLikeFactory.jsm");

const formFillController = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                             .getService(Ci.nsIFormFillController);

const AUTOFILL_FIELDS_THRESHOLD = 3;

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
    let focusedInput = formFillController.focusedInput;
    this.forceStop = false;
    let info = this._serializeInfo(FormAutofillContent.getInputDetails(focusedInput));

    this._getProfiles({info, searchString}).then((profiles) => {
      if (this.forceStop) {
        return;
      }

      let allFieldNames = FormAutofillContent.getAllFieldNames(focusedInput);
      let result = new ProfileAutoCompleteResult(searchString,
                                                 info.fieldName,
                                                 allFieldNames,
                                                 profiles,
                                                 {});

      listener.onSearchResult(this, result);
      ProfileAutocomplete.setProfileAutoCompleteResult(result);
    });
  },

  /**
   * Stops an asynchronous search that is in progress
   */
  stopSearch() {
    ProfileAutocomplete.setProfileAutoCompleteResult(null);
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
  _getProfiles(data) {
    return new Promise((resolve) => {
      Services.cpmm.addMessageListener("FormAutofill:Profiles", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:Profiles", getResult);
        resolve(result.data);
      });

      Services.cpmm.sendAsyncMessage("FormAutofill:GetProfiles", data);
    });
  },

  _serializeInfo(detail) {
    let info = Object.assign({}, detail);
    delete info.element;
    return info;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AutofillProfileAutoCompleteSearch]);

let ProfileAutocomplete = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  _lastAutoCompleteResult: null,
  _registered: false,
  _factory: null,

  ensureRegistered() {
    if (this._registered) {
      return;
    }

    this._factory = new AutocompleteFactory();
    this._factory.register(AutofillProfileAutoCompleteSearch);
    this._registered = true;

    Services.obs.addObserver(this, "autocomplete-will-enter-text", false);
  },

  ensureUnregistered() {
    if (!this._registered) {
      return;
    }

    this._factory.unregister();
    this._factory = null;
    this._registered = false;
    this._lastAutoCompleteResult = null;

    Services.obs.removeObserver(this, "autocomplete-will-enter-text");
  },

  setProfileAutoCompleteResult(result) {
    this._lastAutoCompleteResult = result;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "autocomplete-will-enter-text": {
        if (!formFillController.focusedInput) {
          // The observer notification is for autocomplete in a different process.
          break;
        }
        this._fillFromAutocompleteRow(formFillController.focusedInput);
        break;
      }
    }
  },

  _frameMMFromWindow(contentWindow) {
    return contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDocShell)
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIContentFrameMessageManager);
  },

  _fillFromAutocompleteRow(focusedInput) {
    let formDetails = FormAutofillContent.getFormDetails(focusedInput);
    if (!formDetails) {
      // The observer notification is for a different frame.
      return;
    }

    let mm = this._frameMMFromWindow(focusedInput.ownerGlobal);
    let selectedIndexResult = mm.sendSyncMessage("FormAutoComplete:GetSelectedIndex", {});
    if (selectedIndexResult.length != 1 || !Number.isInteger(selectedIndexResult[0])) {
      throw new Error("Invalid autocomplete selectedIndex");
    }
    let selectedIndex = selectedIndexResult[0];

    if (selectedIndex == -1 ||
        !this._lastAutoCompleteResult ||
        this._lastAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile") {
      return;
    }

    let profile = JSON.parse(this._lastAutoCompleteResult.getCommentAt(selectedIndex));

    // TODO: FormAutofillHandler.autofillFormFields will be used for filling
    // fields logic eventually.
    for (let inputInfo of formDetails) {
      // Skip filling the value of focused input which is filled in
      // FormFillController.
      if (inputInfo.element === focusedInput) {
        continue;
      }
      let value = profile[inputInfo.fieldName];
      if (value) {
        inputInfo.element.setUserInput(value);
      }
    }
  },
};

/**
 * Handles content's interactions for the process.
 *
 * NOTE: Declares it by "var" to make it accessible in unit tests.
 */
var FormAutofillContent = {
  _formsDetails: [],

  init() {
    Services.cpmm.addMessageListener("FormAutofill:enabledStatus", (result) => {
      if (result.data) {
        ProfileAutocomplete.ensureRegistered();
      } else {
        ProfileAutocomplete.ensureUnregistered();
      }
    });
    Services.cpmm.sendAsyncMessage("FormAutofill:getEnabledStatus");
    // TODO: use initialProcessData:
    // Services.cpmm.initialProcessData.autofillEnabled
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
          return detail;
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
        return formDetails;
      }
    }
    return null;
  },

  getAllFieldNames(element) {
    let formDetails = this.getFormDetails(element);
    return formDetails.map(record => record.fieldName);
  },

  _identifyAutofillFields(doc) {
    let forms = [];

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
