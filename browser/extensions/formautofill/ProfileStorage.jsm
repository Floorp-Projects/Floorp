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
 *       guid,             // 12 characters
 *
 *       // address fields
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
 *       name,
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

function ProfileStorage(path) {
  this._path = path;
}

ProfileStorage.prototype = {
  // These fields are defined internally for each record.
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
   * Adds a new address.
   *
   * @param {Address} address
   *        The new address for saving.
   */
  add(address) {
    log.debug("add:", address);
    this._store.ensureDataReady();

    let addressToSave = this._clone(address);
    this._normalizeAddress(addressToSave);

    addressToSave.guid = gUUIDGenerator.generateUUID().toString()
                                       .replace(/[{}-]/g, "").substring(0, 12);

    // Metadata
    let now = Date.now();
    addressToSave.timeCreated = now;
    addressToSave.timeLastModified = now;
    addressToSave.timeLastUsed = 0;
    addressToSave.timesUsed = 0;

    this._store.data.addresses.push(addressToSave);

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "add");
  },

  /**
   * Update the specified address.
   *
   * @param  {string} guid
   *         Indicates which address to update.
   * @param  {Address} address
   *         The new address used to overwrite the old one.
   */
  update(guid, address) {
    log.debug("update:", guid, address);
    this._store.ensureDataReady();

    let addressFound = this._findByGUID(guid);
    if (!addressFound) {
      throw new Error("No matching record.");
    }

    let addressToUpdate = this._clone(address);
    this._normalizeAddress(addressToUpdate);

    for (let field of VALID_FIELDS) {
      if (addressToUpdate[field] !== undefined) {
        addressFound[field] = addressToUpdate[field];
      } else {
        delete addressFound[field];
      }
    }

    addressFound.timeLastModified = Date.now();

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "update");
  },

  /**
   * Notifies the stroage of the use of the specified address, so we can update
   * the metadata accordingly.
   *
   * @param  {string} guid
   *         Indicates which address to be notified.
   */
  notifyUsed(guid) {
    this._store.ensureDataReady();

    let addressFound = this._findByGUID(guid);
    if (!addressFound) {
      throw new Error("No matching record.");
    }

    addressFound.timesUsed++;
    addressFound.timeLastUsed = Date.now();

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "notifyUsed");
  },

  /**
   * Removes the specified address. No error occurs if the address isn't found.
   *
   * @param  {string} guid
   *         Indicates which address to remove.
   */
  remove(guid) {
    log.debug("remove:", guid);
    this._store.ensureDataReady();

    this._store.data.addresses =
      this._store.data.addresses.filter(address => address.guid != guid);
    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "remove");
  },

  /**
   * Returns the address with the specified GUID.
   *
   * @param   {string} guid
   *          Indicates which address to retrieve.
   * @returns {Address}
   *          A clone of the address.
   */
  get(guid) {
    log.debug("get:", guid);
    this._store.ensureDataReady();

    let addressFound = this._findByGUID(guid);
    if (!addressFound) {
      throw new Error("No matching record.");
    }

    // The record is cloned to avoid accidental modifications from outside.
    let clonedAddress = this._clone(addressFound);
    this._computeFields(clonedAddress);
    return clonedAddress;
  },

  /**
   * Returns all addresses.
   *
   * @returns {Array.<Address>}
   *          An array containing clones of all addresses.
   */
  getAll() {
    log.debug("getAll");
    this._store.ensureDataReady();

    // Records are cloned to avoid accidental modifications from outside.
    let clonedAddresses = this._store.data.addresses.map(this._clone);
    clonedAddresses.forEach(this._computeFields);
    return clonedAddresses;
  },

  /**
   * Returns the filtered addresses based on input's information and searchString.
   *
   * @returns {Array.<Address>}
   *          An array containing clones of matched addresses.
   */
  getByFilter({info, searchString}) {
    log.debug("getByFilter:", info, searchString);

    let lcSearchString = searchString.toLowerCase();
    let result = this.getAll().filter(address => {
      // Return true if string is not provided and field exists.
      // TODO: We'll need to check if the address is for billing or shipping.
      //       (Bug 1358941)
      let name = address[info.fieldName];

      if (!searchString) {
        return !!name;
      }

      return name.toLowerCase().startsWith(lcSearchString);
    });

    log.debug("getByFilter: Returning", result.length, "result(s)");
    return result;
  },

  _clone(record) {
    return Object.assign({}, record);
  },

  _findByGUID(guid) {
    return this._store.data.addresses.find(address => address.guid == guid);
  },

  _computeFields(address) {
    // Compute name
    address.name = FormAutofillNameUtils.joinNameParts({
      given: address["given-name"],
      middle: address["additional-name"],
      family: address["family-name"],
    });

    // Compute address
    if (address["street-address"]) {
      let streetAddress = address["street-address"].split("\n");
      // TODO: we should prevent the dataloss by concatenating the rest of lines
      //       with a locale-specific character in the future (bug 1360114).
      for (let i = 0; i < 3; i++) {
        if (streetAddress[i]) {
          address["address-line" + (i + 1)] = streetAddress[i];
        }
      }
    }
  },

  _normalizeAddressLines(address) {
    if (address["address-line1"] || address["address-line2"] ||
        address["address-line3"]) {
      // Treat "street-address" as "address-line1" if it contains only one line
      // and "address-line1" is omitted.
      if (!address["address-line1"] && address["street-address"] &&
          !address["street-address"].includes("\n")) {
        address["address-line1"] = address["street-address"];
        delete address["street-address"];
      }

      // Remove "address-line*" but keep the values.
      let addressLines = [1, 2, 3].map(i => {
        let value = address["address-line" + i];
        delete address["address-line" + i];
        return value;
      });

      // Concatenate "address-line*" if "street-address" is omitted.
      if (!address["street-address"]) {
        address["street-address"] = addressLines.join("\n");
      }
    }
  },

  _normalizeName(address) {
    if (!address.name) {
      return;
    }

    let nameParts = FormAutofillNameUtils.splitName(address.name);
    if (!address["given-name"] && nameParts.given) {
      address["given-name"] = nameParts.given;
    }
    if (!address["additional-name"] && nameParts.middle) {
      address["additional-name"] = nameParts.middle;
    }
    if (!address["family-name"] && nameParts.family) {
      address["family-name"] = nameParts.family;
    }
    delete address.name;
  },

  _normalizeAddress(address) {
    this._normalizeName(address);
    this._normalizeAddressLines(address);

    for (let key in address) {
      if (!VALID_FIELDS.includes(key)) {
        throw new Error(`"${key}" is not a valid field.`);
      }
      if (typeof address[key] !== "string" &&
          typeof address[key] !== "number") {
        throw new Error(`"${key}" contains invalid data type.`);
      }
    }
  },

  _dataPostProcessor(data) {
    data.version = SCHEMA_VERSION;
    if (!data.addresses) {
      data.addresses = [];
    }
    return data;
  },

  // For test only.
  _saveImmediately() {
    return this._store._save();
  },
};
