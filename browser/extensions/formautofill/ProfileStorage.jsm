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
 *       version,              // schema version in integer
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
 *       tel,                  // Stored in E.164 format
 *       email,
 *
 *       // computed fields (These fields are computed based on the above fields
 *       // and are not allowed to be modified directly.)
 *       name,
 *       address-line1,
 *       address-line2,
 *       address-line3,
 *       country-name,
 *       tel-country-code,
 *       tel-national,
 *       tel-area-code,
 *       tel-local,
 *       tel-local-prefix,
 *       tel-local-suffix,
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
 *       version,              // schema version in integer
 *
 *       // credit card fields
 *       cc-name,
 *       cc-number-encrypted,
 *       cc-number-masked,     // e.g. ************1234
 *       cc-exp-month,
 *       cc-exp-year,          // 2-digit year will be converted to 4 digits
 *                             // upon saving
 *
 *       // computed fields (These fields are computed based on the above fields
 *       // and are not allowed to be modified directly.)
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
XPCOMUtils.defineLazyModuleGetter(this, "PhoneNumber",
                                  "resource://formautofill/phonenumberutils/PhoneNumber.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyGetter(this, "REGION_NAMES", function() {
  let regionNames = {};
  let countries = Services.strings.createBundle("chrome://global/locale/regionNames.properties").getSimpleEnumeration();
  while (countries.hasMoreElements()) {
    let country = countries.getNext().QueryInterface(Components.interfaces.nsIPropertyElement);
    regionNames[country.key.toUpperCase()] = country.value;
  }
  return regionNames;
});

const PROFILE_JSON_FILE_NAME = "autofill-profiles.json";

const STORAGE_SCHEMA_VERSION = 1;
const ADDRESS_SCHEMA_VERSION = 1;
const CREDIT_CARD_SCHEMA_VERSION = 1;

const VALID_ADDRESS_FIELDS = [
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

const STREET_ADDRESS_COMPONENTS = [
  "address-line1",
  "address-line2",
  "address-line3",
];

const TEL_COMPONENTS = [
  "tel-country-code",
  "tel-national",
  "tel-area-code",
  "tel-local",
  "tel-local-prefix",
  "tel-local-suffix",
];

const VALID_ADDRESS_COMPUTED_FIELDS = [
  "name",
  "country-name",
].concat(STREET_ADDRESS_COMPONENTS, TEL_COMPONENTS);

const VALID_CREDIT_CARD_FIELDS = [
  "cc-name",
  "cc-number-encrypted",
  "cc-number-masked",
  "cc-exp-month",
  "cc-exp-year",
];

const VALID_CREDIT_CARD_COMPUTED_FIELDS = [
  "cc-given-name",
  "cc-additional-name",
  "cc-family-name",
];

const INTERNAL_FIELDS = [
  "guid",
  "version",
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
   * @param {Array.<string>} validComputedFields
   *        A list containing computed field names.
   * @param {number} schemaVersion
   *        The schema version for the new record.
   */
  constructor(store, collectionName, validFields, validComputedFields, schemaVersion) {
    FormAutofillUtils.defineLazyLogGetter(this, "AutofillRecords:" + collectionName);

    this.VALID_FIELDS = validFields;
    this.VALID_COMPUTED_FIELDS = validComputedFields;

    this._store = store;
    this._collectionName = collectionName;
    this._schemaVersion = schemaVersion;

    let hasChanges = (result, record) => this._migrateRecord(record) || result;
    if (this._store.data[this._collectionName].reduce(hasChanges, false)) {
      this._store.saveSoon();
    }
  }

  /**
   * Gets the schema version number.
   *
   * @returns {number}
   *          The current schema version number.
   */
  get version() {
    return this._schemaVersion;
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
    let recordToSave;
    if (record.deleted) {
      if (!record.guid) {
        throw new Error("you must specify the GUID when creating a tombstone");
      }
      if (this._findByGUID(record.guid, {includeDeleted: true})) {
        throw new Error("a record with this GUID already exists");
      }
      recordToSave = {
        guid: record.guid,
        timeLastModified: record.timeLastModified || Date.now(),
        deleted: true,
      };
    } else {
      recordToSave = this._clone(record);
      this._normalizeRecord(recordToSave);

      let guid;
      while (!guid || this._findByGUID(guid)) {
        guid = gUUIDGenerator.generateUUID().toString()
                             .replace(/[{}-]/g, "").substring(0, 12);
      }
      recordToSave.guid = guid;
      recordToSave.version = this.version;

      // Metadata
      let now = Date.now();
      recordToSave.timeCreated = now;
      recordToSave.timeLastModified = now;
      recordToSave.timeLastUsed = 0;
      recordToSave.timesUsed = 0;
    }

    this._computeFields(recordToSave);

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

    this._stripComputedFields(recordFound);
    this._computeFields(recordFound);

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

    let index = this._findIndexByGUID(guid);
    if (index == -1) {
      this.log.warn("attempting to remove non-existing entry", guid);
      return;
    }
    // replace the record with a tombstone.
    this._store.data[this._collectionName][index] = {
      guid,
      timeLastModified: Date.now(),
      deleted: true,
    };
    this._store.saveSoon();

    Services.obs.notifyObservers(null, "formautofill-storage-changed", "remove");
  }

  /**
   * Returns the record with the specified GUID.
   *
   * @param   {string} guid
   *          Indicates which record to retrieve.
   * @param   {boolean} [options.rawData = false]
   *          Returns a raw record without modifications and the computed fields.
   * @returns {Object}
   *          A clone of the record.
   */
  get(guid, {rawData = false} = {}) {
    this.log.debug("get:", guid, rawData);

    let recordFound = this._findByGUID(guid);
    if (!recordFound) {
      return null;
    }

    // The record is cloned to avoid accidental modifications from outside.
    let clonedRecord = this._clone(recordFound);
    if (rawData) {
      this._stripComputedFields(clonedRecord);
    } else {
      this._recordReadProcessor(clonedRecord);
    }
    return clonedRecord;
  }

  /**
   * Returns all records.
   *
   * @param   {boolean} [options.rawData = false]
   *          Returns raw records without modifications and the computed fields.
   * @param   {boolean} [options.includeDeleted = false]
   *          Also return any tombstone records.
   * @returns {Array.<Object>}
   *          An array containing clones of all records.
   */
  getAll({rawData = false, includeDeleted = false} = {}) {
    this.log.debug("getAll", rawData, includeDeleted);

    let records = this._store.data[this._collectionName].filter(r => !r.deleted || includeDeleted);
    // Records are cloned to avoid accidental modifications from outside.
    let clonedRecords = records.map(this._clone);
    clonedRecords.forEach(record => {
      if (rawData) {
        this._stripComputedFields(record);
      } else {
        this._recordReadProcessor(record);
      }
    });
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

  _findByGUID(guid, {includeDeleted = false} = {}) {
    let found = this._findIndexByGUID(guid, {includeDeleted});
    return found < 0 ? undefined : this._store.data[this._collectionName][found];
  }

  _findIndexByGUID(guid, {includeDeleted = false} = {}) {
    return this._store.data[this._collectionName].findIndex(record => {
      return record.guid == guid && (!record.deleted || includeDeleted);
    });
  }

  _migrateRecord(record) {
    let hasChanges = false;

    if (!record.version || isNaN(record.version) || record.version < 1) {
      this.log.warn("Invalid record version:", record.version);

      // Force to run the migration.
      record.version = 0;
    }

    if (record.version < this.version) {
      hasChanges = true;
      record.version = this.version;

      // Force to recompute fields if we upgrade the schema.
      this._stripComputedFields(record);
    }

    hasChanges |= this._computeFields(record);
    return hasChanges;
  }

  _normalizeRecord(record) {
    this._normalizeFields(record);

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

  _stripComputedFields(record) {
    this.VALID_COMPUTED_FIELDS.forEach(field => delete record[field]);
  }

  // An interface to be inherited.
  _recordReadProcessor(record) {}

  // An interface to be inherited.
  _computeFields(record) {}

  // An interface to be inherited.
  _normalizeFields(record) {}

  // An interface to be inherited.
  mergeIfPossible(guid, record) {}

  // An interface to be inherited.
  mergeToStorage(targetRecord) {}
}

class Addresses extends AutofillRecords {
  constructor(store) {
    super(store, "addresses", VALID_ADDRESS_FIELDS, VALID_ADDRESS_COMPUTED_FIELDS, ADDRESS_SCHEMA_VERSION);
  }

  _recordReadProcessor(address) {
    // TODO: We only support US in MVP so hide the field if it's not. We
    //       are going to support more countries in bug 1370193.
    if (address.country && address.country != "US") {
      address["country-name"] = "";
      delete address.country;
    }
  }

  _computeFields(address) {
    // NOTE: Remember to bump the schema version number if any of the existing
    //       computing algorithm changes. (No need to bump when just adding new
    //       computed fields)

    let hasNewComputedFields = false;

    // Compute name
    if (!("name" in address)) {
      let name = FormAutofillNameUtils.joinNameParts({
        given: address["given-name"],
        middle: address["additional-name"],
        family: address["family-name"],
      });
      address.name = name;
      hasNewComputedFields = true;
    }

    // Compute address lines
    if (!("address-line1" in address)) {
      let streetAddress = [];
      if (address["street-address"]) {
        streetAddress = address["street-address"].split("\n").map(s => s.trim());
      }
      for (let i = 0; i < 3; i++) {
        address["address-line" + (i + 1)] = streetAddress[i] || "";
      }
      if (streetAddress.length > 3) {
        address["address-line3"] = FormAutofillUtils.toOneLineAddress(
          streetAddress.splice(2)
        );
      }
      hasNewComputedFields = true;
    }

    // Compute country name
    if (!("country-name" in address)) {
      if (address.country && REGION_NAMES[address.country]) {
        address["country-name"] = REGION_NAMES[address.country];
      } else {
        address["country-name"] = "";
      }
      hasNewComputedFields = true;
    }

    // Compute tel
    if (!("tel-national" in address)) {
      if (address.tel) {
        // Set "US" as the default region as we only support "en-US" for now.
        let browserCountryCode = Services.prefs.getCharPref("browser.search.countryCode", "US");
        let tel = PhoneNumber.Parse(address.tel, address.country || browserCountryCode);
        if (tel) {
          if (tel.countryCode) {
            address["tel-country-code"] = tel.countryCode;
          }
          if (tel.nationalNumber) {
            address["tel-national"] = tel.nationalNumber;
          }

          // PhoneNumberUtils doesn't support parsing the components of a telephone
          // number so we hard coded the parser for US numbers only. We will need
          // to figure out how to parse numbers from other regions when we support
          // new countries in the future.
          if (tel.nationalNumber && tel.countryCode == "+1") {
            let telComponents = tel.nationalNumber.match(/(\d{3})((\d{3})(\d{4}))$/);
            if (telComponents) {
              address["tel-area-code"] = telComponents[1];
              address["tel-local"] = telComponents[2];
              address["tel-local-prefix"] = telComponents[3];
              address["tel-local-suffix"] = telComponents[4];
            }
          }
        } else {
          // Treat "tel" as "tel-national" directly if it can't be parsed.
          address["tel-national"] = address.tel;
        }
      }

      TEL_COMPONENTS.forEach(c => {
        address[c] = address[c] || "";
      });
    }

    return hasNewComputedFields;
  }

  _normalizeFields(address) {
    this._normalizeName(address);
    this._normalizeAddress(address);
    this._normalizeCountry(address);
    this._normalizeTel(address);
  }

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
  }

  _normalizeAddress(address) {
    if (STREET_ADDRESS_COMPONENTS.every(c => !address[c])) {
      return;
    }

    // Treat "street-address" as "address-line1" if it contains only one line
    // and "address-line1" is omitted.
    if (!address["address-line1"] && address["street-address"] &&
        !address["street-address"].includes("\n")) {
      address["address-line1"] = address["street-address"];
      delete address["street-address"];
    }

    // Concatenate "address-line*" if "street-address" is omitted.
    if (!address["street-address"]) {
      address["street-address"] = STREET_ADDRESS_COMPONENTS.map(c => address[c]).join("\n");
    }

    STREET_ADDRESS_COMPONENTS.forEach(c => delete address[c]);
  }

  _normalizeCountry(address) {
    if (address.country) {
      let country = address.country.toUpperCase();
      // Only values included in the region list will be saved.
      if (REGION_NAMES[country]) {
        address.country = country;
      } else {
        delete address.country;
      }
    } else if (address["country-name"]) {
      for (let region in REGION_NAMES) {
        if (REGION_NAMES[region].toLowerCase() == address["country-name"].toLowerCase()) {
          address.country = region;
          break;
        }
      }
    }
    delete address["country-name"];
  }

  _normalizeTel(address) {
    if (!address.tel && TEL_COMPONENTS.every(c => !address[c])) {
      return;
    }

    // Set "US" as the default region as we only support "en-US" for now.
    let browserCountryCode = Services.prefs.getCharPref("browser.search.countryCode", "US");
    let region = address["tel-country-code"] || address.country || browserCountryCode;
    let number;

    if (address.tel) {
      number = address.tel;
    } else if (address["tel-national"]) {
      number = address["tel-national"];
    } else if (address["tel-local"]) {
      number = (address["tel-area-code"] || "") + address["tel-local"];
    } else if (address["tel-local-prefix"] && address["tel-local-suffix"]) {
      number = (address["tel-area-code"] || "") + address["tel-local-prefix"] + address["tel-local-suffix"];
    }

    let tel = PhoneNumber.Parse(number, region);
    if (tel) {
      // Force to save numbers in E.164 format if parse success.
      address.tel = tel.internationalNumber;
    } else if (!address.tel) {
      // Save the original number anyway if "tel" is omitted.
      address.tel = number;
    }

    TEL_COMPONENTS.forEach(c => delete address[c]);
  }

  /**
   * Merge new address into the specified address if mergeable.
   *
   * @param  {string} guid
   *         Indicates which address to merge.
   * @param  {Object} address
   *         The new address used to merge into the old one.
   * @returns {boolean}
   *          Return true if address is merged into target with specific guid or false if not.
   */
  mergeIfPossible(guid, address) {
    this.log.debug("mergeIfPossible:", guid, address);

    let addressFound = this._findByGUID(guid);
    if (!addressFound) {
      throw new Error("No matching address.");
    }

    let addressToMerge = this._clone(address);
    this._normalizeRecord(addressToMerge);
    let hasMatchingField = false;

    for (let field of this.VALID_FIELDS) {
      if (addressToMerge[field] !== undefined && addressFound[field] !== undefined) {
        if (addressToMerge[field] != addressFound[field]) {
          this.log.debug("Conflicts: field", field, "has different value.");
          return false;
        }
        hasMatchingField = true;
      }
    }

    // We merge the address only when at least one field has the same value.
    if (!hasMatchingField) {
      this.log.debug("Unable to merge because no field has the same value");
      return false;
    }

    // Early return if the data is the same.
    let exactlyMatch = this.VALID_FIELDS.every((field) =>
      addressFound[field] === addressToMerge[field]
    );
    if (exactlyMatch) {
      return true;
    }

    for (let field in addressToMerge) {
      if (this.VALID_FIELDS.includes(field)) {
        addressFound[field] = addressToMerge[field];
      }
    }

    addressFound.timeLastModified = Date.now();

    this._stripComputedFields(addressFound);
    this._computeFields(addressFound);

    this._store.saveSoon();
    let str = Cc["@mozilla.org/supports-string;1"]
                 .createInstance(Ci.nsISupportsString);
    str.data = guid;
    Services.obs.notifyObservers(str, "formautofill-storage-changed", "merge");
    return true;
  }

  /**
   * Merge the address if storage has multiple mergeable records.
   * @param {Object} targetAddress
   *        The address for merge.
   * @returns {Array.<string>}
   *          Return an array of the merged GUID string.
   */
  mergeToStorage(targetAddress) {
    let mergedGUIDs = [];
    for (let address of this._store.data[this._collectionName]) {
      if (!address.deleted && this.mergeIfPossible(address.guid, targetAddress)) {
        mergedGUIDs.push(address.guid);
      }
    }
    this.log.debug("Existing records matching and merging count is", mergedGUIDs.length);
    return mergedGUIDs;
  }
}

class CreditCards extends AutofillRecords {
  constructor(store) {
    super(store, "creditCards", VALID_CREDIT_CARD_FIELDS, VALID_CREDIT_CARD_COMPUTED_FIELDS, CREDIT_CARD_SCHEMA_VERSION);
  }

  _computeFields(creditCard) {
    // NOTE: Remember to bump the schema version number if any of the existing
    //       computing algorithm changes. (No need to bump when just adding new
    //       computed fields)

    let hasNewComputedFields = false;

    // Compute split names
    if (!("cc-given-name" in creditCard)) {
      let nameParts = FormAutofillNameUtils.splitName(creditCard["cc-name"]);
      creditCard["cc-given-name"] = nameParts.given;
      creditCard["cc-additional-name"] = nameParts.middle;
      creditCard["cc-family-name"] = nameParts.family;
      hasNewComputedFields = true;
    }

    return hasNewComputedFields;
  }

  _normalizeFields(creditCard) {
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
  get version() {
    return STORAGE_SCHEMA_VERSION;
  },

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
    data.version = this.version;
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
