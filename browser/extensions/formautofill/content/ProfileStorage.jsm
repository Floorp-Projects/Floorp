/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implements an interface of the storage of Form Autofill.
 *
 * The data is stored in JSON format, without indentation, using UTF-8 encoding.
 * With indentation applied, the file would look like this:
 *
 * {
 *   version: 1,
 *   profiles: [
 *     {
 *       guid,             // 12 character...
 *
 *       // profile
 *       organization,     // Company
 *       streetAddress,    // (Multiline)
 *       addressLevel2,    // City/Town
 *       addressLevel1,    // Province (Standardized code if possible)
 *       postalCode,
 *       country,          // ISO 3166
 *       tel,
 *       email,
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

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const SCHEMA_VERSION = 1;

// Name-related fields will be handled in follow-up bugs due to the complexity.
const VALID_FIELDS = [
  "organization",
  "streetAddress",
  "addressLevel2",
  "addressLevel1",
  "postalCode",
  "country",
  "tel",
  "email",
];

function ProfileStorage(path) {
  this._path = path;
}

ProfileStorage.prototype = {
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
    this._store.ensureDataReady();

    let profileToSave = this._normalizeProfile(profile);

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
    this._store.ensureDataReady();

    let profileFound = this._findByGUID(guid);
    if (!profileFound) {
      throw new Error("No matching profile.");
    }

    let profileToUpdate = this._normalizeProfile(profile);
    for (let field of VALID_FIELDS) {
      if (profileToUpdate[field] !== undefined) {
        profileFound[field] = profileToUpdate[field];
      } else {
        delete profileFound[field];
      }
    }

    profileFound.timeLastModified = Date.now();

    this._store.saveSoon();
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
  },

  /**
   * Removes the specified profile. No error occurs if the profile isn't found.
   *
   * @param  {string} guid
   *         Indicates which profile to remove.
   */
  remove(guid) {
    this._store.ensureDataReady();

    this._store.data.profiles =
      this._store.data.profiles.filter(profile => profile.guid != guid);
    this._store.saveSoon();
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
    this._store.ensureDataReady();

    let profileFound = this._findByGUID(guid);
    if (!profileFound) {
      throw new Error("No matching profile.");
    }

    // Profile is cloned to avoid accidental modifications from outside.
    return this._clone(profileFound);
  },

  /**
   * Returns all profiles.
   *
   * @returns {Array.<Profile>}
   *          An array containing clones of all profiles.
   */
  getAll() {
    this._store.ensureDataReady();

    // Profiles are cloned to avoid accidental modifications from outside.
    return this._store.data.profiles.map(this._clone);
  },

  _clone(profile) {
    return Object.assign({}, profile);
  },

  _findByGUID(guid) {
    return this._store.data.profiles.find(profile => profile.guid == guid);
  },

  _normalizeProfile(profile) {
    let result = {};
    for (let key in profile) {
      if (!VALID_FIELDS.includes(key)) {
        throw new Error(`"${key}" is not a valid field.`);
      }
      if (typeof profile[key] !== "string" &&
          typeof profile[key] !== "number") {
        throw new Error(`"${key}" contains invalid data type.`);
      }

      result[key] = profile[key];
    }
    return result;
  },

  _dataPostProcessor(data) {
    data.version = SCHEMA_VERSION;
    if (!data.profiles) {
      data.profiles = [];
    }
    return data;
  },

  // For test only.
  _saveImmediately() {
    return this._store._save();
  },
};

this.EXPORTED_SYMBOLS = ["ProfileStorage"];
