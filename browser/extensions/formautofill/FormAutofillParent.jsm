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

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ProfileStorage",
                                  "resource://formautofill/ProfileStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillPreferences",
                                  "resource://formautofill/FormAutofillPreferences.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const PROFILE_JSON_FILE_NAME = "autofill-profiles.json";
const ENABLED_PREF = "browser.formautofill.enabled";

function FormAutofillParent() {
}

FormAutofillParent.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  _profileStore: null,

  /**
   * Whether Form Autofill is enabled in preferences.
   * Caches the latest value of this._getStatus().
   */
  _enabled: false,

  /**
   * Initializes ProfileStorage and registers the message handler.
   */
  init() {
    log.debug("init");
    let storePath = OS.Path.join(OS.Constants.Path.profileDir, PROFILE_JSON_FILE_NAME);
    this._profileStore = new ProfileStorage(storePath);
    this._profileStore.initialize();

    Services.obs.addObserver(this, "advanced-pane-loaded", false);
    Services.ppmm.addMessageListener("FormAutofill:GetProfiles", this);
    Services.ppmm.addMessageListener("FormAutofill:SaveProfile", this);
    Services.ppmm.addMessageListener("FormAutofill:RemoveProfiles", this);

    // Observing the pref and storage changes
    Services.prefs.addObserver(ENABLED_PREF, this, false);
    Services.obs.addObserver(this, "formautofill-storage-changed", false);

    // Force to trigger the onStatusChanged function for setting listeners properly
    // while initizlization
    this._setStatus(this._getStatus());
    this._updateSavedFieldNames();
  },

  observe(subject, topic, data) {
    log.debug("observe:", topic, "with data:", data);
    switch (topic) {
      case "advanced-pane-loaded": {
        let formAutofillPreferences = new FormAutofillPreferences();
        let document = subject.document;
        let prefGroup = formAutofillPreferences.init(document);
        let parentNode = document.getElementById("mainPrefPane");
        let insertBeforeNode = document.getElementById("locationBarGroup");
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

    return this._profileStore.getAll().length > 0;
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
      case "FormAutofill:GetProfiles": {
        this._getProfiles(data, target);
        break;
      }
      case "FormAutofill:SaveProfile": {
        if (data.guid) {
          this.getProfileStore().update(data.guid, data.profile);
        } else {
          this.getProfileStore().add(data.profile);
        }
        break;
      }
      case "FormAutofill:RemoveProfiles": {
        data.guids.forEach(guid => this.getProfileStore().remove(guid));
        break;
      }
    }
  },

  /**
   * Returns the instance of ProfileStorage. To avoid syncing issues, anyone
   * who needs to access the profile should request the instance by this instead
   * of creating a new one.
   *
   * @returns {ProfileStorage}
   */
  getProfileStore() {
    return this._profileStore;
  },

  /**
   * Uninitializes FormAutofillParent. This is for testing only.
   *
   * @private
   */
  _uninit() {
    if (this._profileStore) {
      this._profileStore._saveImmediately();
      this._profileStore = null;
    }

    Services.ppmm.removeMessageListener("FormAutofill:GetProfiles", this);
    Services.ppmm.removeMessageListener("FormAutofill:SaveProfile", this);
    Services.ppmm.removeMessageListener("FormAutofill:RemoveProfiles", this);
    Services.obs.removeObserver(this, "advanced-pane-loaded");
    Services.prefs.removeObserver(ENABLED_PREF, this);
  },

  /**
   * Get the profile data from profile store and return profiles back to content process.
   *
   * @private
   * @param  {string} data.searchString
   *         The typed string for filtering out the matched profile.
   * @param  {string} data.info
   *         The input autocomplete property's information.
   * @param  {nsIFrameMessageManager} target
   *         Content's message manager.
   */
  _getProfiles({searchString, info}, target) {
    let profiles = [];

    if (info && info.fieldName) {
      profiles = this._profileStore.getByFilter({searchString, info});
    } else {
      profiles = this._profileStore.getAll();
    }

    target.sendAsyncMessage("FormAutofill:Profiles", profiles);
  },

  _updateSavedFieldNames() {
    if (!Services.ppmm.initialProcessData.autofillSavedFieldNames) {
      Services.ppmm.initialProcessData.autofillSavedFieldNames = new Set();
    } else {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.clear();
    }

    this._profileStore.getAll().forEach((profile) => {
      Object.keys(profile).forEach((fieldName) => {
        if (!profile[fieldName]) {
          return;
        }
        Services.ppmm.initialProcessData.autofillSavedFieldNames.add(fieldName);
      });
    });

    // Remove the internal guid and metadata fields.
    this._profileStore.INTERNAL_FIELDS.forEach((fieldName) => {
      Services.ppmm.initialProcessData.autofillSavedFieldNames.delete(fieldName);
    });

    Services.ppmm.broadcastAsyncMessage("FormAutofill:savedFieldNames",
                                        Services.ppmm.initialProcessData.autofillSavedFieldNames);
  },
};
