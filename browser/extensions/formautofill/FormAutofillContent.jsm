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

Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddressResult",
                                  "resource://formautofill/ProfileAutoCompleteResult.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CreditCardResult",
                                  "resource://formautofill/ProfileAutoCompleteResult.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillHandler",
                                  "resource://formautofill/FormAutofillHandler.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormLikeFactory",
                                  "resource://gre/modules/FormLikeFactory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "InsecurePasswordUtils",
                                  "resource://gre/modules/InsecurePasswordUtils.jsm");

const formFillController = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                             .getService(Ci.nsIFormFillController);
const autocompleteController = Cc["@mozilla.org/autocomplete/controller;1"]
                             .getService(Ci.nsIAutoCompleteController);

const {ADDRESSES_COLLECTION_NAME, CREDITCARDS_COLLECTION_NAME, FIELD_STATES} = FormAutofillUtils;

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

    this.forceStop = false;

    let savedFieldNames = FormAutofillContent.savedFieldNames;

    let focusedInput = formFillController.focusedInput;
    let info = FormAutofillContent.getInputDetails(focusedInput);
    let isAddressField = FormAutofillUtils.isAddressField(info.fieldName);
    let isInputAutofilled = info.state == FIELD_STATES.AUTO_FILLED;
    let handler = FormAutofillContent.getFormHandler(focusedInput);
    let activeSection = handler.activeSection;
    let allFieldNames = activeSection.allFieldNames;
    let filledRecordGUID = activeSection.getFilledRecordGUID();
    let searchPermitted = isAddressField ?
                          FormAutofillUtils.isAutofillAddressesEnabled :
                          FormAutofillUtils.isAutofillCreditCardsEnabled;
    let AutocompleteResult = isAddressField ? AddressResult : CreditCardResult;

    ProfileAutocomplete.lastProfileAutoCompleteFocusedInput = focusedInput;
    // Fallback to form-history if ...
    //   - specified autofill feature is pref off.
    //   - no profile can fill the currently-focused input.
    //   - the current form has already been populated.
    //   - (address only) less than 3 inputs are covered by all saved fields in the storage.
    if (!searchPermitted || !savedFieldNames.has(info.fieldName) ||
        (!isInputAutofilled && filledRecordGUID) || (isAddressField &&
        allFieldNames.filter(field => savedFieldNames.has(field)).length < FormAutofillUtils.AUTOFILL_FIELDS_THRESHOLD)) {
      if (focusedInput.autocomplete == "off") {
        // Create a dummy result as an empty search result.
        let result = new AutocompleteResult("", "", [], [], {});
        listener.onSearchResult(this, result);
        return;
      }
      let formHistory = Cc["@mozilla.org/autocomplete/search;1?name=form-history"]
                          .createInstance(Ci.nsIAutoCompleteSearch);
      formHistory.startSearch(searchString, searchParam, previousResult, {
        onSearchResult: (search, result) => {
          listener.onSearchResult(this, result);
          ProfileAutocomplete.lastProfileAutoCompleteResult = result;
        },
      });
      return;
    }

    if (isInputAutofilled) {
      let result = new AutocompleteResult(searchString, "", [], [], {isInputAutofilled});
      listener.onSearchResult(this, result);
      ProfileAutocomplete.lastProfileAutoCompleteResult = result;
      return;
    }

    let infoWithoutElement = Object.assign({}, info);
    delete infoWithoutElement.elementWeakRef;

    let data = {
      collectionName: isAddressField ? ADDRESSES_COLLECTION_NAME : CREDITCARDS_COLLECTION_NAME,
      info: infoWithoutElement,
      searchString,
    };

    this._getRecords(data).then((records) => {
      if (this.forceStop) {
        return;
      }
      // Sort addresses by timeLastUsed for showing the lastest used address at top.
      records.sort((a, b) => b.timeLastUsed - a.timeLastUsed);

      let adaptedRecords = activeSection.getAdaptedProfiles(records);
      let result = null;
      let isSecure = InsecurePasswordUtils.isFormSecure(handler.form);

      result = new AutocompleteResult(searchString,
                                      info.fieldName,
                                      allFieldNames,
                                      adaptedRecords,
                                      {isSecure, isInputAutofilled});
      listener.onSearchResult(this, result);
      ProfileAutocomplete.lastProfileAutoCompleteResult = result;
    });
  },

  /**
   * Stops an asynchronous search that is in progress
   */
  stopSearch() {
    ProfileAutocomplete.lastProfileAutoCompleteResult = null;
    this.forceStop = true;
  },

  /**
   * Get the records from parent process for AutoComplete result.
   *
   * @private
   * @param  {Object} data
   *         Parameters for querying the corresponding result.
   * @param  {string} data.collectionName
   *         The name used to specify which collection to retrieve records.
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched records.
   * @param  {string} data.info
   *         The input autocomplete property's information.
   * @returns {Promise}
   *          Promise that resolves when addresses returned from parent process.
   */
  _getRecords(data) {
    this.log.debug("_getRecords with data:", data);
    return new Promise((resolve) => {
      Services.cpmm.addMessageListener("FormAutofill:Records", function getResult(result) {
        Services.cpmm.removeMessageListener("FormAutofill:Records", getResult);
        resolve(result.data);
      });

      Services.cpmm.sendAsyncMessage("FormAutofill:GetRecords", data);
    });
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AutofillProfileAutoCompleteSearch]);

let ProfileAutocomplete = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  lastProfileAutoCompleteResult: null,
  lastProfileAutoCompleteFocusedInput: null,
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

    Services.obs.addObserver(this, "autocomplete-will-enter-text");
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
        !this.lastProfileAutoCompleteResult ||
        this.lastProfileAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile") {
      return;
    }

    let profile = JSON.parse(this.lastProfileAutoCompleteResult.getCommentAt(selectedIndex));
    let {fieldName} = FormAutofillContent.getInputDetails(focusedInput);
    let formHandler = FormAutofillContent.getFormHandler(focusedInput);

    formHandler.autofillFormFields(profile).then(() => {
      autocompleteController.searchString = profile[fieldName];
    });
  },

  _clearProfilePreview() {
    let focusedInput = formFillController.focusedInput || this.lastProfileAutoCompleteFocusedInput;
    if (!focusedInput || !FormAutofillContent.getFormDetails(focusedInput)) {
      return;
    }

    let formHandler = FormAutofillContent.getFormHandler(focusedInput);

    formHandler.activeSection.clearPreviewedFormFields();
  },

  _previewSelectedProfile(selectedIndex) {
    let focusedInput = formFillController.focusedInput;
    if (!focusedInput || !FormAutofillContent.getFormDetails(focusedInput)) {
      // The observer notification is for a different process/frame.
      return;
    }

    if (!this.lastProfileAutoCompleteResult ||
        this.lastProfileAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile") {
      return;
    }

    let profile = JSON.parse(this.lastProfileAutoCompleteResult.getCommentAt(selectedIndex));
    let formHandler = FormAutofillContent.getFormHandler(focusedInput);

    formHandler.activeSection.previewFormFields(profile);
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
    Services.obs.addObserver(this, "earlyformsubmit");

    let autofillEnabled = Services.cpmm.initialProcessData.autofillEnabled;
    // If storage hasn't be initialized yet autofillEnabled is undefined but we need to ensure
    // autocomplete is registered before the focusin so register it in this case as long as the
    // pref is true.
    let shouldEnableAutofill = autofillEnabled === undefined &&
                               (FormAutofillUtils.isAutofillAddressesEnabled ||
                               FormAutofillUtils.isAutofillCreditCardsEnabled);
    if (autofillEnabled || shouldEnableAutofill) {
      ProfileAutocomplete.ensureRegistered();
    }

    this.savedFieldNames =
      Services.cpmm.initialProcessData.autofillSavedFieldNames;
  },

  /**
   * Send the profile to parent for doorhanger and storage saving/updating.
   *
   * @param {Object} profile Submitted form's address/creditcard guid and record.
   * @param {Object} domWin Current content window.
   * @param {int} timeStartedFillingMS Time of form filling started.
   */
  _onFormSubmit(profile, domWin, timeStartedFillingMS) {
    let mm = this._messageManagerFromWindow(domWin);
    mm.sendAsyncMessage("FormAutofill:OnFormSubmit",
                        {profile, timeStartedFillingMS});
  },

  /**
   * Handle earlyformsubmit event and early return when:
   * 1. In private browsing mode.
   * 2. Could not map any autofill handler by form element.
   * 3. Number of filled fields is less than autofill threshold
   *
   * @param {HTMLElement} formElement Root element which receives earlyformsubmit event.
   * @param {Object} domWin Content window
   * @returns {boolean} Should always return true so form submission isn't canceled.
   */
  notify(formElement, domWin) {
    this.log.debug("Notifying form early submission");

    if (!FormAutofillUtils.isAutofillEnabled) {
      this.log.debug("Form Autofill is disabled");
      return true;
    }

    if (domWin && PrivateBrowsingUtils.isContentWindowPrivate(domWin)) {
      this.log.debug("Ignoring submission in a private window");
      return true;
    }

    let handler = this._formsDetails.get(formElement);
    if (!handler) {
      this.log.debug("Form element could not map to an existing handler");
      return true;
    }

    let records = handler.createRecords();
    if (!Object.values(records).some(typeRecords => typeRecords.length)) {
      return true;
    }

    this._onFormSubmit(records, domWin, handler.timeStartedFillingMS);
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
    let formHandler = this.getFormHandler(element);
    return formHandler ? formHandler.activeSection.allFieldNames : null;
  },

  identifyAutofillFields(element) {
    this.log.debug("identifyAutofillFields:", "" + element.ownerDocument.location);

    if (!this.savedFieldNames) {
      this.log.debug("identifyAutofillFields: savedFieldNames are not known yet");
      Services.cpmm.sendAsyncMessage("FormAutofill:InitStorage");
    }

    let formHandler = this.getFormHandler(element);
    if (!formHandler) {
      let formLike = FormLikeFactory.createFromField(element);
      formHandler = new FormAutofillHandler(formLike);
    } else if (!formHandler.updateFormIfNeeded(element)) {
      formHandler.focusedInput = element;
      this.log.debug("No control is removed or inserted since last collection.");
      return;
    }

    let validDetails = formHandler.collectFormFields();

    this._formsDetails.set(formHandler.form.rootElement, formHandler);
    this.log.debug("Adding form handler to _formsDetails:", formHandler);

    validDetails.forEach(detail =>
      this._markAsAutofillField(detail.elementWeakRef.get())
    );
    formHandler.focusedInput = element;
  },

  clearForm() {
    let focusedInput = formFillController.focusedInput || ProfileAutocomplete._lastAutoCompleteFocusedInput;
    if (!focusedInput) {
      return;
    }

    let formHandler = this.getFormHandler(focusedInput);
    formHandler.activeSection.clearPopulatedForm();
    autocompleteController.searchString = "";
  },

  previewProfile(doc) {
    let docWin = doc.ownerGlobal;
    let selectedIndex = ProfileAutocomplete._getSelectedIndex(docWin);
    let lastAutoCompleteResult = ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = formFillController.focusedInput;
    let mm = this._messageManagerFromWindow(docWin);

    if (selectedIndex === -1 ||
        !focusedInput ||
        !lastAutoCompleteResult ||
        lastAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile") {
      mm.sendAsyncMessage("FormAutofill:UpdateWarningMessage", {});

      ProfileAutocomplete._clearProfilePreview();
    } else {
      let focusedInputDetails = this.getInputDetails(focusedInput);
      let profile = JSON.parse(lastAutoCompleteResult.getCommentAt(selectedIndex));
      let allFieldNames = FormAutofillContent.getAllFieldNames(focusedInput);
      let profileFields = allFieldNames.filter(fieldName => !!profile[fieldName]);

      let focusedCategory = FormAutofillUtils.getCategoryFromFieldName(focusedInputDetails.fieldName);
      let categories = FormAutofillUtils.getCategoriesFromFieldNames(profileFields);
      mm.sendAsyncMessage("FormAutofill:UpdateWarningMessage", {
        focusedCategory,
        categories,
      });

      ProfileAutocomplete._previewSelectedProfile(selectedIndex);
    }
  },

  onPopupClosed() {
    ProfileAutocomplete._clearProfilePreview();
  },

  _markAsAutofillField(field) {
    // Since Form Autofill popup is only for input element, any non-Input
    // element should be excluded here.
    if (!field || !(field instanceof Ci.nsIDOMHTMLInputElement)) {
      return;
    }

    formFillController.markAsAutofillField(field);
  },

  _messageManagerFromWindow(win) {
    return win.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIWebNavigation)
              .QueryInterface(Ci.nsIDocShell)
              .QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIContentFrameMessageManager);
  },

  _onKeyDown(e) {
    let lastAutoCompleteResult = ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = formFillController.focusedInput;

    if (e.keyCode != Ci.nsIDOMKeyEvent.DOM_VK_RETURN || !lastAutoCompleteResult ||
        !focusedInput || focusedInput != ProfileAutocomplete.lastProfileAutoCompleteFocusedInput) {
      return;
    }

    let selectedIndex = ProfileAutocomplete._getSelectedIndex(e.target.ownerGlobal);
    let selectedRowStyle = lastAutoCompleteResult.getStyleAt(selectedIndex);
    focusedInput.addEventListener("DOMAutoComplete", () => {
      if (selectedRowStyle == "autofill-footer") {
        Services.cpmm.sendAsyncMessage("FormAutofill:OpenPreferences");
      } else if (selectedRowStyle == "autofill-clear-button") {
        FormAutofillContent.clearForm();
      }
    }, {once: true});
  },
};


FormAutofillContent.init();
