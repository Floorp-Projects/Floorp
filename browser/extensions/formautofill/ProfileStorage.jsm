/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements an interface of the storage of Form Autofill.
 *
 * The data is stored in JSON format, without indentation and the computed
 * fields, using UTF-8 encoding. With indentation and computed fields applied,
 * the schema would look like this:
 *
 * {
 *   version: 1,
 *   profiles: [
 *     {
 *       guid,             // 12 character...
 *
 *       // profile
 *       given-name,
 *       additional-name,
 *       family-name,
 *       organization,     // Company
 *       street-address,   // (Multiline)
 *       address-level2,   // City/Town
 *       address-level1,   // Province (Standardized code if possible)
 *       postal-code,
 *       country,          // ISO 3166
 *       tel,
 *       email,
 *
 *       // computed fields (These fields are not stored in the file as they are
 *       // generated at runtime.)
 *       address-line1,
 *       address-line2,
 *       address-line3,
 *
 *       // metadata
 *       timeCreated,      // in ms
 *       timeLastUsed,     // in ms
 *       timeLastModified, // in ms
 *       timesUsed
 *     },
 *     {
 *       // ...
 *     }
 *   ]
 * }
 */

"use strict";

this.EXPORTED_SYMBOLS = ["ProfileStorage"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillNameUtils",
                                  "resource://formautofill/FormAutofillNameUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

const SCHEMA_VERSION = 1;

const VALID_FIELDS = [
  "given-name",
  "additional-name",
  "family-name",
  "organization",
  "street-address",
  "address-level2",
  "address-level1",
  "postal-code",
  "country",
  "tel",
  "email",
];

// TODO: Remove this once we can add profile from preference.
const MOCK_MODE = false;
const MOCK_STORAGE = [{
  guid: "test-guid-1",
  organization: "Sesame Street",
  "street-address": "123 Sesame Street.",
  tel: "1-345-345-3456",
}, {
  guid: "test-guid-2",
  organization: "Mozilla",
  "street-address": "331 E. Evelyn Avenue",
  tel: "1-650-903-0800",
}];

function ProfileStorage(path) {
  this._path = path;
}

ProfileStorage.prototype = {
  // These fields are defined internally for each profile.
  INTERNAL_FIELDS:
    ["guid", "timeCreated", "timeLastUsed", "timeLastModified", "timesUsed"],
  /**
   * Loads the profile data from file to memory.
   *
   * @returns {Promise}
   * @resolves When the operation finished successfully.
   * @rejects  JavaScript exception.
   */
  initialize() {
    this._store = new JSONFile({
      path: this._path,
      dataPostProcessor: this._dataPostProcessor.bind(this),
    });
    return this._store.load();
  },

  /**
   * Adds a new profile.
   *
   * @param {Profile} profile
   *        The new profile for saving.
   */
  add(profile) {
    log.debug("add:", profile);
    this._store.ensureDataReady();

    let profileToSave = this._clone(profile);
    this._normalizeProfile(profileToSave);

    profileToSave.guid = gUUIDGenerator.generateUUID().toString()
                                       .replace(/[{}-]/g, "").substring(0, 12);

    // Metadata
    let now = Date.now();
    profileToSave.timeCreated = now;
    profileToSave.timeLastModified = now;
    profileToSave.timeLastUsed = 0;
    profileToSave.timesUsed = 0;

    this._store.data.profiles.push(profileToSave);

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "add");
  },

  /**
   * Update the specified profile.
   *
   * @param  {string} guid
   *         Indicates which profile to update.
   * @param  {Profile} profile
   *         The new profile used to overwrite the old one.
   */
  update(guid, profile) {
    log.debug("update:", guid, profile);
    this._store.ensureDataReady();

    let profileFound = this._findByGUID(guid);
    if (!profileFound) {
      throw new Error("No matching profile.");
    }

    let profileToUpdate = this._clone(profile);
    this._normalizeProfile(profileToUpdate);

    for (let field of VALID_FIELDS) {
      if (profileToUpdate[field] !== undefined) {
        profileFound[field] = profileToUpdate[field];
      } else {
        delete profileFound[field];
      }
    }

    profileFound.timeLastModified = Date.now();

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "update");
  },

  /**
   * Notifies the stroage of the use of the specified profile, so we can update
   * the metadata accordingly.
   *
   * @param  {string} guid
   *         Indicates which profile to be notified.
   */
  notifyUsed(guid) {
    this._store.ensureDataReady();

    let profileFound = this._findByGUID(guid);
    if (!profileFound) {
      throw new Error("No matching profile.");
    }

    profileFound.timesUsed++;
    profileFound.timeLastUsed = Date.now();

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "notifyUsed");
  },

  /**
   * Removes the specified profile. No error occurs if the profile isn't found.
   *
   * @param  {string} guid
   *         Indicates which profile to remove.
   */
  remove(guid) {
    log.debug("remove:", guid);
    this._store.ensureDataReady();

    this._store.data.profiles =
      this._store.data.profiles.filter(profile => profile.guid != guid);
    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "remove");
  },

  /**
   * Returns the profile with the specified GUID.
   *
   * @param   {string} guid
   *          Indicates which profile to retrieve.
   * @returns {Profile}
   *          A clone of the profile.
   */
  get(guid) {
    log.debug("get:", guid);
    this._store.ensureDataReady();

    let profileFound = this._findByGUID(guid);
    if (!profileFound) {
      throw new Error("No matching profile.");
    }

    // Profile is cloned to avoid accidental modifications from outside.
    let clonedProfile = this._clone(profileFound);
    this._computeFields(clonedProfile);
    return clonedProfile;
  },

  /**
   * Returns all profiles.
   *
   * @returns {Array.<Profile>}
   *          An array containing clones of all profiles.
   */
  getAll() {
    log.debug("getAll");
    this._store.ensureDataReady();

    // Profiles are cloned to avoid accidental modifications from outside.
    let clonedProfiles = this._store.data.profiles.map(this._clone);
    clonedProfiles.forEach(this._computeFields);
    return clonedProfiles;
  },

  /**
   * Returns the filtered profiles based on input's information and searchString.
   *
   * @returns {Array.<Profile>}
   *          An array containing clones of matched profiles.
   */
  getByFilter({info, searchString}) {
    log.debug("getByFilter:", info, searchString);

    let lcSearchString = searchString.toLowerCase();
    let result = this.getAll().filter(profile => {
      // Return true if string is not provided and field exists.
      // TODO: We'll need to check if the address is for billing or shipping.
      //       (Bug 1358941)
      let name = profile[info.fieldName];

      if (!searchString) {
        return !!name;
      }

      return name.toLowerCase().startsWith(lcSearchString);
    });

    log.debug("getByFilter: Returning", result.length, "result(s)");
    return result;
  },

  _clone(profile) {
    return Object.assign({}, profile);
  },

  _findByGUID(guid) {
    return this._store.data.profiles.find(profile => profile.guid == guid);
  },

  _computeFields(profile) {
    if (profile["street-address"]) {
      let streetAddress = profile["street-address"].split("\n");
      // TODO: we should prevent the dataloss by concatenating the rest of lines
      //       with a locale-specific character in the future (bug 1360114).
      for (let i = 0; i < 3; i++) {
        if (streetAddress[i]) {
          profile["address-line" + (i + 1)] = streetAddress[i];
        }
      }
    }
  },

  _normalizeAddress(profile) {
    if (profile["address-line1"] || profile["address-line2"] ||
        profile["address-line3"]) {
      // Treat "street-address" as "address-line1" if it contains only one line
      // and "address-line1" is omitted.
      if (!profile["address-line1"] && profile["street-address"] &&
          !profile["street-address"].includes("\n")) {
        profile["address-line1"] = profile["street-address"];
        delete profile["street-address"];
      }

      // Remove "address-line*" but keep the values.
      let addressLines = [1, 2, 3].map(i => {
        let value = profile["address-line" + i];
        delete profile["address-line" + i];
        return value;
      });

      // Concatenate "address-line*" if "street-address" is omitted.
      if (!profile["street-address"]) {
        profile["street-address"] = addressLines.join("\n");
      }
    }
  },

  _normalizeProfile(profile) {
    this._normalizeAddress(profile);

    for (let key in profile) {
      if (!VALID_FIELDS.includes(key)) {
        throw new Error(`"${key}" is not a valid field.`);
      }
      if (typeof profile[key] !== "string" &&
          typeof profile[key] !== "number") {
        throw new Error(`"${key}" contains invalid data type.`);
      }
    }
  },

  _dataPostProcessor(data) {
    data.version = SCHEMA_VERSION;
    if (!data.profiles) {
      data.profiles = MOCK_MODE ? MOCK_STORAGE : [];
    }
    return data;
  },

  // For test only.
  _saveImmediately() {
    return this._store._save();
  },
};
