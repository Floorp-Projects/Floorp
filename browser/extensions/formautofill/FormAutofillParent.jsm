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

XPCOMUtils.defineLazyModuleGetter(this, "profileStorage",
                                  "resource://formautofill/ProfileStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillPreferences",
                                  "resource://formautofill/FormAutofillPreferences.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const ENABLED_PREF = "browser.formautofill.enabled";

function FormAutofillParent() {
}

FormAutofillParent.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  /**
   * Whether Form Autofill is enabled in preferences.
   * Caches the latest value of this._getStatus().
   */
  _enabled: false,

  /**
   * Initializes ProfileStorage and registers the message handler.
   */
  async init() {
    log.debug("init");
    await profileStorage.initialize();

    Services.obs.addObserver(this, "advanced-pane-loaded");
    Services.ppmm.addMessageListener("FormAutofill:GetAddresses", this);
    Services.ppmm.addMessageListener("FormAutofill:SaveAddress", this);
    Services.ppmm.addMessageListener("FormAutofill:RemoveAddresses", this);

    // Observing the pref and storage changes
    Services.prefs.addObserver(ENABLED_PREF, this);
    Services.obs.addObserver(this, "formautofill-storage-changed");

    // Force to trigger the onStatusChanged function for setting listeners properly
    // while initizlization
    this._setStatus(this._getStatus());
    this._updateSavedFieldNames();
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
                               document.getElementById("passwordGrid");
        parentNode.insertBefore(prefGroup, insertBeforeNode);
        break;
      }

      case "nsPref:changed": {
        // Observe pref changes and update _enabled cache if status is changed.
        let currentStatus = this._getStatus();
        if (currentStatus !== this._enabled) {
          this._setStatus(currentStatus);
        }
        break;
      }

      case "formautofill-storage-changed": {
        // Early exit if the action is not "add" nor "remove"
        if (data != "add" && data != "remove") {
          break;
        }

        this._updateSavedFieldNames();
        let currentStatus = this._getStatus();
        if (currentStatus !== this._enabled) {
          this._setStatus(currentStatus);
        }
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
    log.debug("_onStatusChanged: Status changed to", this._enabled);
    Services.ppmm.broadcastAsyncMessage("FormAutofill:enabledStatus", this._enabled);
    // Sync process data autofillEnabled to make sure the value up to date
    // no matter when the new content process is initialized.
    Services.ppmm.initialProcessData.autofillEnabled = this._enabled;
  },

  /**
   * Query pref and storage status to determine the overall status for
   * form autofill feature.
   *
   * @returns {boolean} status of form autofill feature
   */
  _getStatus() {
    if (!Services.prefs.getBoolPref(ENABLED_PREF)) {
      return false;
    }

    return profileStorage.getAll().length > 0;
  },

  /**
   * Set status and trigger _onStatusChanged.
   *
   * @param {boolean} newStatus The latest status we want to set for _enabled
   */
  _setStatus(newStatus) {
    this._enabled = newStatus;
    this._onStatusChanged();
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
      case "FormAutofill:GetAddresses": {
        this._getAddresses(data, target);
        break;
      }
      case "FormAutofill:SaveAddress": {
        if (data.guid) {
          profileStorage.update(data.guid, data.address);
        } else {
          profileStorage.add(data.address);
        }
        break;
      }
      case "FormAutofill:RemoveAddresses": {
        data.guids.forEach(guid => profileStorage.remove(guid));
        break;
      }
    }
  },

  /**
   * Uninitializes FormAutofillParent. This is for testing only.
   *
   * @private
   */
  _uninit() {
    profileStorage._saveImmediately();

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
      addresses = profileStorage.getByFilter({searchString, info});
    } else {
      addresses = profileStorage.getAll();
    }

    target.sendAsyncMessage("FormAutofill:Addresses", addresses);
  },

  _updateSavedFieldNames() {
    if (!Services.ppmm.initialProcessData.autofillSavedFieldNames) {
      Services.ppmm.initialProcessData.autofillSavedFieldNames = new Set();
    } else {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.clear();
    }

    profileStorage.getAll().forEach((address) => {
      Object.keys(address).forEach((fieldName) => {
        if (!address[fieldName]) {
          return;
        }
        Services.ppmm.initialProcessData.autofillSavedFieldNames.add(fieldName);
      });
    });

    // Remove the internal guid and metadata fields.
    profileStorage.INTERNAL_FIELDS.forEach((fieldName) => {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.delete(fieldName);
    });

    Services.ppmm.broadcastAsyncMessage("FormAutofill:savedFieldNames",
                                        Services.ppmm.initialProcessData.autofillSavedFieldNames);
  },
};
