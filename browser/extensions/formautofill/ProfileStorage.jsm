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
 *   addresses: [
 *     {
 *       guid,                 // 12 characters
 *
 *       // address fields
 *       given-name,
 *       additional-name,
 *       family-name,
 *       organization,         // Company
 *       street-address,       // (Multiline)
 *       address-level2,       // City/Town
 *       address-level1,       // Province (Standardized code if possible)
 *       postal-code,
 *       country,              // ISO 3166
 *       tel,
 *       email,
 *
 *       // computed fields (These fields are not stored in the file as they are
 *       // generated at runtime.)
 *       name,
 *       address-line1,
 *       address-line2,
 *       address-line3,
 *
 *       // metadata
 *       timeCreated,          // in ms
 *       timeLastUsed,         // in ms
 *       timeLastModified,     // in ms
 *       timesUsed
 *     }
 *   ],
 *   creditCards: [
 *     {
 *       guid,                 // 12 characters
 *
 *       // credit card fields
 *       cc-name,
 *       cc-number-encrypted,
 *       cc-number-masked,     // e.g. ************1234
 *       cc-exp-month,
 *       cc-exp-year,          // 2-digit year will be converted to 4 digits
 *                             // upon saving
 *
 *       // computed fields (These fields are not stored in the file as they are
 *       // generated at runtime.)
 *       cc-given-name,
 *       cc-additional-name,
 *       cc-family-name,
 *
 *       // metadata
 *       timeCreated,          // in ms
 *       timeLastUsed,         // in ms
 *       timeLastModified,     // in ms
 *       timesUsed
 *     }
 *   ]
 * }
 */

"use strict";

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
this.EXPORTED_SYMBOLS = ["profileStorage"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSONFile",
                                  "resource://gre/modules/JSONFile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillNameUtils",
                                  "resource://formautofill/FormAutofillNameUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const PROFILE_JSON_FILE_NAME = "autofill-profiles.json";

const SCHEMA_VERSION = 1;

const VALID_PROFILE_FIELDS = [
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

const VALID_CREDIT_CARD_FIELDS = [
  "cc-name",
  "cc-number-encrypted",
  "cc-number-masked",
  "cc-exp-month",
  "cc-exp-year",
];

const INTERNAL_FIELDS = [
  "guid",
  "timeCreated",
  "timeLastUsed",
  "timeLastModified",
  "timesUsed",
];

/**
 * Class that manipulates records in a specified collection.
 *
 * Note that it is responsible for converting incoming data to a consistent
 * format in the storage. For example, computed fields will be transformed to
 * the original fields and 2-digit years will be calculated into 4 digits.
 */
class AutofillRecords {
  /**
   * Creates an AutofillRecords.
   *
   * @param {JSONFile} store
   *        An instance of JSONFile.
   * @param {string} collectionName
   *        A key of "store.data".
   * @param {Array.<string>} validFields
   *        A list containing non-metadata field names.
   */
  constructor(store, collectionName, validFields) {
    FormAutofillUtils.defineLazyLogGetter(this, "AutofillRecords:" + collectionName);

    this.VALID_FIELDS = validFields;

    this._store = store;
    this._collectionName = collectionName;
  }

  /**
   * Gets the schema version number.
   *
   * @returns {number}
   *          The current schema version number.
   */
  get version() {
    return SCHEMA_VERSION;
  }

  /**
   * Adds a new record.
   *
   * @param {Object} record
   *        The new record for saving.
   * @returns {string}
   *          The GUID of the newly added item..
   */
  add(record) {
    this.log.debug("add:", record);

    let recordToSave = this._clone(record);
    this._normalizeRecord(recordToSave);

    let guid;
    while (!guid || this._findByGUID(guid)) {
      guid = gUUIDGenerator.generateUUID().toString()
                           .replace(/[{}-]/g, "").substring(0, 12);
    }
    recordToSave.guid = guid;

    // Metadata
    let now = Date.now();
    recordToSave.timeCreated = now;
    recordToSave.timeLastModified = now;
    recordToSave.timeLastUsed = 0;
    recordToSave.timesUsed = 0;

    this._store.data[this._collectionName].push(recordToSave);
    this._store.saveSoon();

    Services.obs.notifyObservers(null, "formautofill-storage-changed", "add");
    return recordToSave.guid;
  }

  /**
   * Update the specified record.
   *
   * @param  {string} guid
   *         Indicates which record to update.
   * @param  {Object} record
   *         The new record used to overwrite the old one.
   */
  update(guid, record) {
    this.log.debug("update:", guid, record);

    let recordFound = this._findByGUID(guid);
    if (!recordFound) {
      throw new Error("No matching record.");
    }

    let recordToUpdate = this._clone(record);
    this._normalizeRecord(recordToUpdate);
    for (let field of this.VALID_FIELDS) {
      if (recordToUpdate[field] !== undefined) {
        recordFound[field] = recordToUpdate[field];
      } else {
        delete recordFound[field];
      }
    }

    recordFound.timeLastModified = Date.now();

    this._store.saveSoon();

    Services.obs.notifyObservers(null, "formautofill-storage-changed", "update");
  }

  /**
   * Notifies the stroage of the use of the specified record, so we can update
   * the metadata accordingly.
   *
   * @param  {string} guid
   *         Indicates which record to be notified.
   */
  notifyUsed(guid) {
    this.log.debug("notifyUsed:", guid);

    let recordFound = this._findByGUID(guid);
    if (!recordFound) {
      throw new Error("No matching record.");
    }

    recordFound.timesUsed++;
    recordFound.timeLastUsed = Date.now();

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "notifyUsed");
  }

  /**
   * Removes the specified record. No error occurs if the record isn't found.
   *
   * @param  {string} guid
   *         Indicates which record to remove.
   */
  remove(guid) {
    this.log.debug("remove:", guid);

    this._store.data[this._collectionName] =
      this._store.data[this._collectionName].filter(record => record.guid != guid);
    this._store.saveSoon();

    Services.obs.notifyObservers(null, "formautofill-storage-changed", "remove");
  }

  /**
   * Returns the record with the specified GUID.
   *
   * @param   {string} guid
   *          Indicates which record to retrieve.
   * @returns {Object}
   *          A clone of the record.
   */
  get(guid) {
    this.log.debug("get:", guid);

    let recordFound = this._findByGUID(guid);
    if (!recordFound) {
      return null;
    }

    // The record is cloned to avoid accidental modifications from outside.
    let clonedRecord = this._clone(recordFound);
    this._recordReadProcessor(clonedRecord);
    return clonedRecord;
  }

  /**
   * Returns all records.
   *
   * @param   {Object} config
   *          Specifies how data will be retrieved.
   * @param   {boolean} config.noComputedFields
   *          Returns raw record without those computed fields.
   * @returns {Array.<Object>}
   *          An array containing clones of all records.
   */
  getAll(config = {}) {
    this.log.debug("getAll", config);

    // Records are cloned to avoid accidental modifications from outside.
    let clonedRecords = this._store.data[this._collectionName].map(this._clone);
    clonedRecords.forEach(record => this._recordReadProcessor(record, config));
    return clonedRecords;
  }

  /**
   * Returns the filtered records based on input's information and searchString.
   *
   * @returns {Array.<Object>}
   *          An array containing clones of matched record.
   */
  getByFilter({info, searchString}) {
    this.log.debug("getByFilter:", info, searchString);

    let lcSearchString = searchString.toLowerCase();
    let result = this.getAll().filter(record => {
      // Return true if string is not provided and field exists.
      // TODO: We'll need to check if the address is for billing or shipping.
      //       (Bug 1358941)
      let name = record[info.fieldName];

      if (!searchString) {
        return !!name;
      }

      return name && name.toLowerCase().startsWith(lcSearchString);
    });

    this.log.debug("getByFilter:", "Returning", result.length, "result(s)");
    return result;
  }

  _clone(record) {
    return Object.assign({}, record);
  }

  _findByGUID(guid) {
    let found = this._findIndexByGUID(guid);
    return found < 0 ? undefined : this._store.data[this._collectionName][found];
  }

  _findIndexByGUID(guid) {
    return this._store.data[this._collectionName].findIndex(record => record.guid == guid);
  }

  _normalizeRecord(record) {
    this._recordWriteProcessor(record);

    for (let key in record) {
      if (!this.VALID_FIELDS.includes(key)) {
        throw new Error(`"${key}" is not a valid field.`);
      }
      if (typeof record[key] !== "string" &&
          typeof record[key] !== "number") {
        throw new Error(`"${key}" contains invalid data type.`);
      }
    }
  }

  // An interface to be inherited.
  _recordReadProcessor(record, config) {}

  // An interface to be inherited.
  _recordWriteProcessor(record) {}
}

class Addresses extends AutofillRecords {
  constructor(store) {
    super(store, "addresses", VALID_PROFILE_FIELDS);
  }

  _recordReadProcessor(profile, {noComputedFields} = {}) {
    if (noComputedFields) {
      return;
    }

    // Compute name
    let name = FormAutofillNameUtils.joinNameParts({
      given: profile["given-name"],
      middle: profile["additional-name"],
      family: profile["family-name"],
    });
    if (name) {
      profile.name = name;
    }

    // Compute address
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
  }

  _recordWriteProcessor(profile) {
    // Normalize name
    if (profile.name) {
      let nameParts = FormAutofillNameUtils.splitName(profile.name);
      if (!profile["given-name"] && nameParts.given) {
        profile["given-name"] = nameParts.given;
      }
      if (!profile["additional-name"] && nameParts.middle) {
        profile["additional-name"] = nameParts.middle;
      }
      if (!profile["family-name"] && nameParts.family) {
        profile["family-name"] = nameParts.family;
      }
      delete profile.name;
    }

    // Normalize address
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
  }
}

class CreditCards extends AutofillRecords {
  constructor(store) {
    super(store, "creditCards", VALID_CREDIT_CARD_FIELDS);
  }

  _recordReadProcessor(creditCard, {noComputedFields} = {}) {
    if (noComputedFields) {
      return;
    }

    // Compute split names
    if (creditCard["cc-name"]) {
      let nameParts = FormAutofillNameUtils.splitName(creditCard["cc-name"]);
      if (nameParts.given) {
        creditCard["cc-given-name"] = nameParts.given;
      }
      if (nameParts.middle) {
        creditCard["cc-additional-name"] = nameParts.middle;
      }
      if (nameParts.family) {
        creditCard["cc-family-name"] = nameParts.family;
      }
    }
  }

  _recordWriteProcessor(creditCard) {
    // Fields that should not be set by content.
    delete creditCard["cc-number-encrypted"];
    delete creditCard["cc-number-masked"];

    // Validate and encrypt credit card numbers, and calculate the masked numbers
    if (creditCard["cc-number"]) {
      let ccNumber = creditCard["cc-number"].replace(/\s/g, "");
      delete creditCard["cc-number"];

      if (!/^\d+$/.test(ccNumber)) {
        throw new Error("Credit card number contains invalid characters.");
      }

      // TODO: Encrypt cc-number here (bug 1337314).
      // e.g. creditCard["cc-number-encrypted"] = Encrypt(creditCard["cc-number"]);

      if (ccNumber.length > 4) {
        creditCard["cc-number-masked"] = "*".repeat(ccNumber.length - 4) + ccNumber.substr(-4);
      } else {
        creditCard["cc-number-masked"] = ccNumber;
      }
    }

    // Normalize name
    if (creditCard["cc-given-name"] || creditCard["cc-additional-name"] || creditCard["cc-family-name"]) {
      if (!creditCard["cc-name"]) {
        creditCard["cc-name"] = FormAutofillNameUtils.joinNameParts({
          given: creditCard["cc-given-name"],
          middle: creditCard["cc-additional-name"],
          family: creditCard["cc-family-name"],
        });
      }

      delete creditCard["cc-given-name"];
      delete creditCard["cc-additional-name"];
      delete creditCard["cc-family-name"];
    }

    // Validate expiry date
    if (creditCard["cc-exp-month"]) {
      let expMonth = parseInt(creditCard["cc-exp-month"], 10);
      if (isNaN(expMonth) || expMonth < 1 || expMonth > 12) {
        delete creditCard["cc-exp-month"];
      } else {
        creditCard["cc-exp-month"] = expMonth;
      }
    }
    if (creditCard["cc-exp-year"]) {
      let expYear = parseInt(creditCard["cc-exp-year"], 10);
      if (isNaN(expYear) || expYear < 0) {
        delete creditCard["cc-exp-year"];
      } else if (expYear < 100) {
        // Enforce 4 digits years.
        creditCard["cc-exp-year"] = expYear + 2000;
      } else {
        creditCard["cc-exp-year"] = expYear;
      }
    }
  }
}

function ProfileStorage(path) {
  this._path = path;
  this._initializePromise = null;
  this.INTERNAL_FIELDS = INTERNAL_FIELDS;
}

ProfileStorage.prototype = {
  get addresses() {
    if (!this._addresses) {
      this._store.ensureDataReady();
      this._addresses = new Addresses(this._store);
    }
    return this._addresses;
  },

  get creditCards() {
    if (!this._creditCards) {
      this._store.ensureDataReady();
      this._creditCards = new CreditCards(this._store);
    }
    return this._creditCards;
  },

  /**
   * Loads the profile data from file to memory.
   *
   * @returns {Promise}
   * @resolves When the operation finished successfully.
   * @rejects  JavaScript exception.
   */
  initialize() {
    if (!this._initializePromise) {
      this._store = new JSONFile({
        path: this._path,
        dataPostProcessor: this._dataPostProcessor.bind(this),
      });
      this._initializePromise = this._store.load();
    }
    return this._initializePromise;
  },

  _dataPostProcessor(data) {
    data.version = SCHEMA_VERSION;
    if (!data.addresses) {
      data.addresses = [];
    }
    if (!data.creditCards) {
      data.creditCards = [];
    }
    return data;
  },

  // For test only.
  _saveImmediately() {
    return this._store._save();
  },
};

// The singleton exposed by this module.
this.profileStorage = new ProfileStorage(
  OS.Path.join(OS.Constants.Path.profileDir, PROFILE_JSON_FILE_NAME));
