/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements a service used to access storage and communicate with content.
 *
 * A "fields" array is used to communicate with FormAutofillContent. Each item
 * represents a single input field in the content page as well as its
 * @autocomplete properties. The schema is as below. Please refer to
 * FormAutofillContent.jsm for more details.
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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ProfileStorage",
                                  "resource://formautofill/ProfileStorage.jsm");

const PROFILE_JSON_FILE_NAME = "autofill-profiles.json";

let FormAutofillParent = {
  _profileStore: null,

  /**
   * Initializes ProfileStorage and registers the message handler.
   */
  init: function() {
    let storePath =
      OS.Path.join(OS.Constants.Path.profileDir, PROFILE_JSON_FILE_NAME);

    this._profileStore = new ProfileStorage(storePath);
    this._profileStore.initialize();

    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);
    mm.addMessageListener("FormAutofill:PopulateFieldValues", this);
  },

  /**
   * Handles the message coming from FormAutofillContent.
   *
   * @param   {string} message.name The name of the message.
   * @param   {object} message.data The data of the message.
   * @param   {nsIFrameMessageManager} message.target Caller's message manager.
   */
  receiveMessage: function({name, data, target}) {
    switch (name) {
      case "FormAutofill:PopulateFieldValues":
        this._populateFieldValues(data, target);
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
  getProfileStore: function() {
    return this._profileStore;
  },

  /**
   * Uninitializes FormAutofillParent. This is for testing only.
   *
   * @private
   */
  _uninit: function() {
    if (this._profileStore) {
      this._profileStore._saveImmediately();
      this._profileStore = null;
    }

    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);
    mm.removeMessageListener("FormAutofill:PopulateFieldValues", this);
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
   * Transforms a word with hyphen into camel case.
   * (e.g. transforms "address-type" into "addressType".)
   *
   * @private
   * @param   {string} str The original string with hyphen.
   * @returns {string} The camel-cased output string.
   */
  _camelCase(str) {
    return str.toLowerCase().replace(/-([a-z])/g, s => s[1].toUpperCase());
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
    let key = this._camelCase(fieldName);

    // TODO: Transform the raw profile data to fulfill "fieldName" here.

    return profile[key];
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
