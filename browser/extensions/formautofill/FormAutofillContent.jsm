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
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

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
  FormAutofillUtils.defineLazyLogGetter(this, "AutofillProfileAutoCompleteSearch");
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
    this.log.debug("startSearch: for", searchString, "with input", formFillController.focusedInput);
    let focusedInput = formFillController.focusedInput;
    this.forceStop = false;
    let info = FormAutofillContent.getInputDetails(focusedInput);

    if (!FormAutofillContent.savedFieldNames.has(info.fieldName) ||
        FormAutofillContent.getFormHandler(focusedInput).filledProfileGUID) {
      let formHistory = Cc["@mozilla.org/autocomplete/search;1?name=form-history"]
                          .createInstance(Ci.nsIAutoCompleteSearch);
      formHistory.startSearch(searchString, searchParam, previousResult, {
        onSearchResult: (search, result) => {
          listener.onSearchResult(this, result);
          ProfileAutocomplete.setProfileAutoCompleteResult(result);
        },
      });
      return;
    }

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
    this.log.debug("_getProfiles with data:", data);
    return new Promise((resolve) => {
      Services.cpmm.addMessageListener("FormAutofill:Profiles", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:Profiles", getResult);
        resolve(result.data);
      });

      Services.cpmm.sendAsyncMessage("FormAutofill:GetProfiles", data);
    });
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AutofillProfileAutoCompleteSearch]);

let ProfileAutocomplete = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  _lastAutoCompleteResult: null,
  _lastAutoCompleteFocusedInput: null,
  _registered: false,
  _factory: null,

  ensureRegistered() {
    if (this._registered) {
      return;
    }

    FormAutofillUtils.defineLazyLogGetter(this, "ProfileAutocomplete");
    this.log.debug("ensureRegistered");
    this._factory = new AutocompleteFactory();
    this._factory.register(AutofillProfileAutoCompleteSearch);
    this._registered = true;

    Services.obs.addObserver(this, "autocomplete-will-enter-text", false);
  },

  ensureUnregistered() {
    if (!this._registered) {
      return;
    }

    this.log.debug("ensureUnregistered");
    this._factory.unregister();
    this._factory = null;
    this._registered = false;
    this._lastAutoCompleteResult = null;

    Services.obs.removeObserver(this, "autocomplete-will-enter-text");
  },

  setProfileAutoCompleteResult(result) {
    this._lastAutoCompleteResult = result;
    this._lastAutoCompleteFocusedInput = formFillController.focusedInput;
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

  _getSelectedIndex(contentWindow) {
    let mm = this._frameMMFromWindow(contentWindow);
    let selectedIndexResult = mm.sendSyncMessage("FormAutoComplete:GetSelectedIndex", {});
    if (selectedIndexResult.length != 1 || !Number.isInteger(selectedIndexResult[0])) {
      throw new Error("Invalid autocomplete selectedIndex");
    }

    return selectedIndexResult[0];
  },

  _fillFromAutocompleteRow(focusedInput) {
    this.log.debug("_fillFromAutocompleteRow:", focusedInput);
    let formDetails = FormAutofillContent.getFormDetails(focusedInput);
    if (!formDetails) {
      // The observer notification is for a different frame.
      return;
    }

    let selectedIndex = this._getSelectedIndex(focusedInput.ownerGlobal);
    if (selectedIndex == -1 ||
        !this._lastAutoCompleteResult ||
        this._lastAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile") {
      return;
    }

    let profile = JSON.parse(this._lastAutoCompleteResult.getCommentAt(selectedIndex));
    let formHandler = FormAutofillContent.getFormHandler(focusedInput);

    formHandler.autofillFormFields(profile, focusedInput);
  },

  _clearProfilePreview() {
    let focusedInput = formFillController.focusedInput || this._lastAutoCompleteFocusedInput;
    if (!focusedInput || !FormAutofillContent.getFormDetails(focusedInput)) {
      return;
    }

    let formHandler = FormAutofillContent.getFormHandler(focusedInput);

    formHandler.clearPreviewedFormFields();
  },

  _previewSelectedProfile(selectedIndex) {
    let focusedInput = formFillController.focusedInput;
    if (!focusedInput || !FormAutofillContent.getFormDetails(focusedInput)) {
      // The observer notification is for a different process/frame.
      return;
    }

    if (!this._lastAutoCompleteResult ||
        this._lastAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile") {
      return;
    }

    let profile = JSON.parse(this._lastAutoCompleteResult.getCommentAt(selectedIndex));
    let formHandler = FormAutofillContent.getFormHandler(focusedInput);

    formHandler.previewFormFields(profile);
  },
};

/**
 * Handles content's interactions for the process.
 *
 * NOTE: Declares it by "var" to make it accessible in unit tests.
 */
var FormAutofillContent = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver]),
  /**
   * @type {WeakMap} mapping FormLike root HTML elements to FormAutofillHandler objects.
   */
  _formsDetails: new WeakMap(),

  /**
   * @type {Set} Set of the fields with usable values in any saved profile.
   */
  savedFieldNames: null,

  init() {
    FormAutofillUtils.defineLazyLogGetter(this, "FormAutofillContent");

    Services.cpmm.addMessageListener("FormAutofill:enabledStatus", this);
    Services.cpmm.addMessageListener("FormAutofill:savedFieldNames", this);
    Services.obs.addObserver(this, "earlyformsubmit", false);

    if (Services.cpmm.initialProcessData.autofillEnabled) {
      ProfileAutocomplete.ensureRegistered();
    }

    this.savedFieldNames =
      Services.cpmm.initialProcessData.autofillSavedFieldNames || new Set();
  },

  _onFormSubmit(handler) {
    // TODO: Handle form submit event for profile saving(bug 990219) and metrics(bug 1341569).
  },

  notify(formElement) {
    this.log.debug("notified for form early submission");

    let handler = this._formsDetails.get(formElement);
    if (!handler) {
      this.log.debug("Form element could not map to an existing handler");
    } else {
      this._onFormSubmit(handler);
    }

    return true;
  },

  receiveMessage({name, data}) {
    switch (name) {
      case "FormAutofill:enabledStatus": {
        if (data) {
          ProfileAutocomplete.ensureRegistered();
        } else {
          ProfileAutocomplete.ensureUnregistered();
        }
        break;
      }
      case "FormAutofill:savedFieldNames": {
        this.savedFieldNames = data;
      }
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
    let formDetails = this.getFormDetails(element);
    for (let detail of formDetails) {
      let detailElement = detail.elementWeakRef.get();
      if (detailElement && element == detailElement) {
        return detail;
      }
    }
    return null;
  },

  /**
   * Get the form's handler from cache which is created after page identified.
   *
   * @param {HTMLInputElement} element Focused input which triggered profile searching
   * @returns {Array<Object>|null}
   *          Return target form's handler from content cache
   *          (or return null if the information is not found in the cache).
   *
   */
  getFormHandler(element) {
    let rootElement = FormLikeFactory.findRootForField(element);
    return this._formsDetails.get(rootElement);
  },

  /**
   * Get the form's information from cache which is created after page identified.
   *
   * @param {HTMLInputElement} element Focused input which triggered profile searching
   * @returns {Array<Object>|null}
   *          Return target form's information from content cache
   *          (or return null if the information is not found in the cache).
   *
   */
  getFormDetails(element) {
    let formHandler = this.getFormHandler(element);
    return formHandler ? formHandler.fieldDetails : null;
  },

  getAllFieldNames(element) {
    let formDetails = this.getFormDetails(element);
    return formDetails.map(record => record.fieldName);
  },

  identifyAutofillFields(doc) {
    this.log.debug("identifyAutofillFields:", "" + doc.location);
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

    this.log.debug("Found", forms.length, "forms");

    // Collects the fields that can be autofilled from each form and marks them
    // as autofill fields if the amount is above the threshold.
    forms.forEach(form => {
      let formHandler = new FormAutofillHandler(form);
      formHandler.collectFormFields();
      if (formHandler.fieldDetails.length < AUTOFILL_FIELDS_THRESHOLD) {
        this.log.debug("Ignoring form since it has only", formHandler.fieldDetails.length,
                       "field(s)");
        return;
      }

      this._formsDetails.set(form.rootElement, formHandler);
      this.log.debug("Adding form handler to _formsDetails:", formHandler);
      formHandler.fieldDetails.forEach(detail =>
        this._markAsAutofillField(detail.elementWeakRef.get())
      );
    });
  },

  _markAsAutofillField(field) {
    if (!field) {
      return;
    }

    formFillController.markAsAutofillField(field);
  },

  _previewProfile(doc) {
    let selectedIndex = ProfileAutocomplete._getSelectedIndex(doc.ownerGlobal);

    if (selectedIndex === -1) {
      ProfileAutocomplete._clearProfilePreview();
    } else {
      ProfileAutocomplete._previewSelectedProfile(selectedIndex);
    }
  },
};


FormAutofillContent.init();
