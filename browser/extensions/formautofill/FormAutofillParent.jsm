/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements a service used to access storage and communicate with content.
 *
 * A "fields" array is used to communicate with FormAutofillContent. Each item
 * represents a single input field in the content page as well as its
 * @autocomplete properties. The schema is as below. Please refer to
 * FormAutofillContent.js for more details.
 *
 * [
 *   {
 *     section,
 *     addressType,
 *     contactType,
 *     fieldName,
 *     value,
 *     index
 *   },
 *   {
 *     // ...
 *   }
 * ]
 */

/* exported FormAutofillParent */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillParent"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillPreferences",
                                  "resource://formautofill/FormAutofillPreferences.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const ENABLED_PREF = "extensions.formautofill.addresses.enabled";

function FormAutofillParent() {
  // Lazily load the storage JSM to avoid disk I/O until absolutely needed.
  // Once storage is loaded we need to update saved field names and inform content processes.
  XPCOMUtils.defineLazyGetter(this, "profileStorage", () => {
    let {profileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});
    log.debug("Loading profileStorage");

    profileStorage.initialize().then(function onStorageInitialized() {
      // Update the saved field names to compute the status and update child processes.
      this._updateSavedFieldNames();
    }.bind(this));

    return profileStorage;
  });
}

FormAutofillParent.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  /**
   * Cache of the Form Autofill status (considering preferences and storage).
   */
  _active: null,

  /**
   * Initializes ProfileStorage and registers the message handler.
   */
  async init() {
    Services.obs.addObserver(this, "advanced-pane-loaded");
    Services.ppmm.addMessageListener("FormAutofill:InitStorage", this);
    Services.ppmm.addMessageListener("FormAutofill:GetAddresses", this);
    Services.ppmm.addMessageListener("FormAutofill:SaveAddress", this);
    Services.ppmm.addMessageListener("FormAutofill:RemoveAddresses", this);
    Services.ppmm.addMessageListener("FormAutofill:OnFormSubmit", this);

    // Observing the pref and storage changes
    Services.prefs.addObserver(ENABLED_PREF, this);
    Services.obs.addObserver(this, "formautofill-storage-changed");
  },

  observe(subject, topic, data) {
    log.debug("observe:", topic, "with data:", data);
    switch (topic) {
      case "advanced-pane-loaded": {
        let useOldOrganization = Services.prefs.getBoolPref("browser.preferences.useOldOrganization",
                                                            false);
        let formAutofillPreferences = new FormAutofillPreferences({useOldOrganization});
        let document = subject.document;
        let prefGroup = formAutofillPreferences.init(document);
        let parentNode = useOldOrganization ?
                         document.getElementById("mainPrefPane") :
                         document.getElementById("passwordsGroup");
        let insertBeforeNode = useOldOrganization ?
                               document.getElementById("locationBarGroup") :
                               document.getElementById("masterPasswordRow");
        parentNode.insertBefore(prefGroup, insertBeforeNode);
        break;
      }

      case "nsPref:changed": {
        // Observe pref changes and update _active cache if status is changed.
        this._updateStatus();
        break;
      }

      case "formautofill-storage-changed": {
        // Early exit if the action is not "add" nor "remove"
        if (data != "add" && data != "remove") {
          break;
        }

        this._updateSavedFieldNames();
        break;
      }

      default: {
        throw new Error(`FormAutofillParent: Unexpected topic observed: ${topic}`);
      }
    }
  },

  /**
   * Broadcast the status to frames when the form autofill status changes.
   */
  _onStatusChanged() {
    log.debug("_onStatusChanged: Status changed to", this._active);
    Services.ppmm.broadcastAsyncMessage("FormAutofill:enabledStatus", this._active);
    // Sync process data autofillEnabled to make sure the value up to date
    // no matter when the new content process is initialized.
    Services.ppmm.initialProcessData.autofillEnabled = this._active;
  },

  /**
   * Query preference and storage status to determine the overall status of the
   * form autofill feature.
   *
   * @returns {boolean} whether form autofill is active (enabled and has data)
   */
  _computeStatus() {
    if (!Services.prefs.getBoolPref(ENABLED_PREF)) {
      return false;
    }

    return Services.ppmm.initialProcessData.autofillSavedFieldNames.size > 0;
  },

  /**
   * Update the status and trigger _onStatusChanged, if necessary.
   */
  _updateStatus() {
    let wasActive = this._active;
    this._active = this._computeStatus();
    if (this._active !== wasActive) {
      this._onStatusChanged();
    }
  },

  /**
   * Handles the message coming from FormAutofillContent.
   *
   * @param   {string} message.name The name of the message.
   * @param   {object} message.data The data of the message.
   * @param   {nsIFrameMessageManager} message.target Caller's message manager.
   */
  receiveMessage({name, data, target}) {
    switch (name) {
      case "FormAutofill:InitStorage": {
        this.profileStorage.initialize();
        break;
      }
      case "FormAutofill:GetAddresses": {
        this._getAddresses(data, target);
        break;
      }
      case "FormAutofill:SaveAddress": {
        if (data.guid) {
          this.profileStorage.addresses.update(data.guid, data.address);
        } else {
          this.profileStorage.addresses.add(data.address);
        }
        break;
      }
      case "FormAutofill:RemoveAddresses": {
        data.guids.forEach(guid => this.profileStorage.addresses.remove(guid));
        break;
      }
      case "FormAutofill:OnFormSubmit": {
        this._onFormSubmit(data, target);
      }
    }
  },

  /**
   * Uninitializes FormAutofillParent. This is for testing only.
   *
   * @private
   */
  _uninit() {
    this.profileStorage._saveImmediately();

    Services.ppmm.removeMessageListener("FormAutofill:InitStorage", this);
    Services.ppmm.removeMessageListener("FormAutofill:GetAddresses", this);
    Services.ppmm.removeMessageListener("FormAutofill:SaveAddress", this);
    Services.ppmm.removeMessageListener("FormAutofill:RemoveAddresses", this);
    Services.obs.removeObserver(this, "advanced-pane-loaded");
    Services.prefs.removeObserver(ENABLED_PREF, this);
  },

  /**
   * Get the address data from profile store and return addresses back to content
   * process.
   *
   * @private
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched address.
   * @param  {string} data.info
   *         The input autocomplete property's information.
   * @param  {nsIFrameMessageManager} target
   *         Content's message manager.
   */
  _getAddresses({searchString, info}, target) {
    let addresses = [];

    if (info && info.fieldName) {
      addresses = this.profileStorage.addresses.getByFilter({searchString, info});
    } else {
      addresses = this.profileStorage.addresses.getAll();
    }

    target.sendAsyncMessage("FormAutofill:Addresses", addresses);
  },

  _updateSavedFieldNames() {
    log.debug("_updateSavedFieldNames");
    if (!Services.ppmm.initialProcessData.autofillSavedFieldNames) {
      Services.ppmm.initialProcessData.autofillSavedFieldNames = new Set();
    } else {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.clear();
    }

    this.profileStorage.addresses.getAll().forEach((address) => {
      Object.keys(address).forEach((fieldName) => {
        if (!address[fieldName]) {
          return;
        }
        Services.ppmm.initialProcessData.autofillSavedFieldNames.add(fieldName);
      });
    });

    // Remove the internal guid and metadata fields.
    this.profileStorage.INTERNAL_FIELDS.forEach((fieldName) => {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.delete(fieldName);
    });

    Services.ppmm.broadcastAsyncMessage("FormAutofill:savedFieldNames",
                                        Services.ppmm.initialProcessData.autofillSavedFieldNames);
    this._updateStatus();
  },

  _onFormSubmit(data, target) {
    let {address} = data;

    if (address.guid) {
      // TODO: Show update doorhanger(bug 1303513) and set probe(bug 990200)
      // if (!profileStorage.addresses.mergeIfPossible(address.guid, address.record)) {
      // }
    } else {
      // TODO: Add first time use probe(bug 990199) and doorhanger(bug 1303510)
      // profileStorage.addresses.add(address.record);
    }
  },
};
