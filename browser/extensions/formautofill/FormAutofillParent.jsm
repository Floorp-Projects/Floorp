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

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ProfileStorage",
                                  "resource://formautofill/ProfileStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillPreferences",
                                  "resource://formautofill/FormAutofillPreferences.jsm");

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
    let storePath = OS.Path.join(OS.Constants.Path.profileDir, PROFILE_JSON_FILE_NAME);
    this._profileStore = new ProfileStorage(storePath);
    this._profileStore.initialize();

    Services.obs.addObserver(this, "advanced-pane-loaded", false);

    // Observing the pref (and storage) changes
    Services.prefs.addObserver(ENABLED_PREF, this, false);
    this._enabled = this._getStatus();
    // Force to trigger the onStatusChanged function for setting listeners properly
    // while initizlization
    this._onStatusChanged();
    Services.mm.addMessageListener("FormAutofill:getEnabledStatus", this);
  },

  observe(subject, topic, data) {
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
          this._enabled = currentStatus;
          this._onStatusChanged();
        }
        break;
      }

      default: {
        throw new Error(`FormAutofillParent: Unexpected topic observed: ${topic}`);
      }
    }
  },

  /**
   * Add/remove message listener and broadcast the status to frames while the
   * form autofill status changed.
   */
  _onStatusChanged() {
    if (this._enabled) {
      Services.mm.addMessageListener("FormAutofill:PopulateFieldValues", this);
      Services.mm.addMessageListener("FormAutofill:GetProfiles", this);
    } else {
      Services.mm.removeMessageListener("FormAutofill:PopulateFieldValues", this);
      Services.mm.removeMessageListener("FormAutofill:GetProfiles", this);
    }

    Services.mm.broadcastAsyncMessage("FormAutofill:enabledStatus", this._enabled);
  },

  /**
   * Query pref (and storage) status to determine the overall status for
   * form autofill feature.
   *
   * @returns {boolean} status of form autofill feature
   */
  _getStatus() {
    return Services.prefs.getBoolPref(ENABLED_PREF);
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
      case "FormAutofill:PopulateFieldValues":
        this._populateFieldValues(data, target);
        break;
      case "FormAutofill:GetProfiles":
        this._getProfiles(data, target);
        break;
      case "FormAutofill:getEnabledStatus":
        target.messageManager.sendAsyncMessage("FormAutofill:enabledStatus",
                                               this._enabled);
        break;
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

    Services.mm.removeMessageListener("FormAutofill:PopulateFieldValues", this);
    Services.mm.removeMessageListener("FormAutofill:GetProfiles", this);
    Services.obs.removeObserver(this, "advanced-pane-loaded");
    Services.prefs.removeObserver(ENABLED_PREF, this);
  },

  /**
   * Populates the field values and notifies content to fill in. Exception will
   * be thrown if there's no matching profile.
   *
   * @private
   * @param  {string} data.guid
   *         Indicates which profile to populate
   * @param  {Fields} data.fields
   *         The "fields" array collected from content.
   * @param  {nsIFrameMessageManager} target
   *         Content's message manager.
   */
  _populateFieldValues({guid, fields}, target) {
    this._profileStore.notifyUsed(guid);
    this._fillInFields(this._profileStore.get(guid), fields);
    target.sendAsyncMessage("FormAutofill:fillForm", {fields});
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

    target.messageManager.sendAsyncMessage("FormAutofill:Profiles", profiles);
  },

  /**
   * Get the corresponding value from the specified profile according to a valid
   * @autocomplete field name.
   *
   * Note that the field name doesn't need to match the property name defined in
   * Profile object. This method can transform the raw data to fulfill it. (e.g.
   * inputting "country-name" as "fieldName" will get a full name transformed
   * from the country code that is recorded in "country" field.)
   *
   * @private
   * @param   {Profile} profile   The specified profile.
   * @param   {string}  fieldName A valid @autocomplete field name.
   * @returns {string}  The corresponding value. Returns "undefined" if there's
   *                    no matching field.
   */
  _getDataByFieldName(profile, fieldName) {
    // TODO: Transform the raw profile data to fulfill "fieldName" here.
    return profile[fieldName];
  },

  /**
   * Fills in the "fields" array by the specified profile.
   *
   * @private
   * @param   {Profile} profile The specified profile to fill in.
   * @param   {Fields}  fields  The "fields" array collected from content.
   */
  _fillInFields(profile, fields) {
    for (let field of fields) {
      let value = this._getDataByFieldName(profile, field.fieldName);
      if (value !== undefined) {
        field.value = value;
      }
    }
  },
};

this.EXPORTED_SYMBOLS = ["FormAutofillParent"];
