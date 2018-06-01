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
 *       _sync: { ... optional sync metadata },
 *     }
 *   ],
 *   creditCards: [
 *     {
 *       guid,                 // 12 characters
 *       version,              // schema version in integer
 *
 *       // credit card fields
 *       billingAddressGUID,   // An optional GUID of an autofill address record
 *                                which may or may not exist locally.
 *
 *       cc-name,
 *       cc-number,            // will be stored in masked format (************1234)
 *                             // (see details below)
 *       cc-exp-month,
 *       cc-exp-year,          // 2-digit year will be converted to 4 digits
 *                             // upon saving
 *
 *       // computed fields (These fields are computed based on the above fields
 *       // and are not allowed to be modified directly.)
 *       cc-given-name,
 *       cc-additional-name,
 *       cc-family-name,
 *       cc-number-encrypted,  // encrypted from the original unmasked "cc-number"
 *                             // (see details below)
 *       cc-exp,
 *
 *       // metadata
 *       timeCreated,          // in ms
 *       timeLastUsed,         // in ms
 *       timeLastModified,     // in ms
 *       timesUsed
 *       _sync: { ... optional sync metadata },
 *     }
 *   ]
 * }
 *
 *
 * Encrypt-related Credit Card Fields (cc-number & cc-number-encrypted):
 *
 * When saving or updating a credit-card record, the storage will encrypt the
 * value of "cc-number", store the encrypted number in "cc-number-encrypted"
 * field, and replace "cc-number" field with the masked number. These all happen
 * in "_computeFields". We do reverse actions in "_stripComputedFields", which
 * decrypts "cc-number-encrypted", restores it to "cc-number", and deletes
 * "cc-number-encrypted". Therefore, calling "_stripComputedFields" followed by
 * "_computeFields" can make sure the encrypt-related fields are up-to-date.
 *
 * In general, you have to decrypt the number by your own outside FormAutofillStorage
 * when necessary. However, you will get the decrypted records when querying
 * data with "rawData=true" to ensure they're ready to sync.
 *
 *
 * Sync Metadata:
 *
 * Records may also have a _sync field, which consists of:
 * {
 *   changeCounter,    // integer - the number of changes made since the last
 *                     // sync.
 *   lastSyncedFields, // object - hashes of the original values for fields
 *                     // changed since the last sync.
 * }
 *
 * Records with such a field have previously been synced. Records without such
 * a field are yet to be synced, so are treated specially in some cases (eg,
 * they don't need a tombstone, de-duping logic treats them as special etc).
 * Records without the field are always considered "dirty" from Sync's POV
 * (meaning they will be synced on the next sync), at which time they will gain
 * this new field.
 */

"use strict";

// We expose a singleton from this module. Some tests may import the
// constructor via a backstage pass.
this.EXPORTED_SYMBOLS = ["formAutofillStorage"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");

ChromeUtils.import("resource://formautofill/FormAutofillUtils.jsm");

ChromeUtils.defineModuleGetter(this, "JSONFile",
                               "resource://gre/modules/JSONFile.jsm");
ChromeUtils.defineModuleGetter(this, "FormAutofillNameUtils",
                               "resource://formautofill/FormAutofillNameUtils.jsm");
ChromeUtils.defineModuleGetter(this, "MasterPassword",
                               "resource://formautofill/MasterPassword.jsm");
ChromeUtils.defineModuleGetter(this, "PhoneNumber",
                               "resource://formautofill/phonenumberutils/PhoneNumber.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUUIDGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const CryptoHash = Components.Constructor("@mozilla.org/security/hash;1",
                                          "nsICryptoHash", "initWithString");

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
  "billingAddressGUID",
  "cc-name",
  "cc-number",
  "cc-exp-month",
  "cc-exp-year",
];

const VALID_CREDIT_CARD_COMPUTED_FIELDS = [
  "cc-given-name",
  "cc-additional-name",
  "cc-family-name",
  "cc-number-encrypted",
  "cc-exp",
];

const INTERNAL_FIELDS = [
  "guid",
  "version",
  "timeCreated",
  "timeLastUsed",
  "timeLastModified",
  "timesUsed",
];

function sha512(string) {
  if (string == null) {
    return null;
  }
  let encoder = new TextEncoder("utf-8");
  let bytes = encoder.encode(string);
  let hash = new CryptoHash("sha512");
  hash.update(bytes, bytes.length);
  return hash.finish(/* base64 */ true);
}

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
    if (this._data.reduce(hasChanges, false)) {
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
   * Gets the data of this collection.
   *
   * @returns {array}
   *          The data object.
   */
  get _data() {
    return this._store.data[this._collectionName];
  }

  // Ensures that we don't try to apply synced records with newer schema
  // versions. This is a temporary measure to ensure we don't accidentally
  // bump the schema version without a syncing strategy in place (bug 1377204).
  _ensureMatchingVersion(record) {
    if (record.version != this.version) {
      throw new Error(`Got unknown record version ${
        record.version}; want ${this.version}`);
    }
  }

  /**
   * Adds a new record.
   *
   * @param {Object} record
   *        The new record for saving.
   * @param {boolean} [options.sourceSync = false]
   *        Did sync generate this addition?
   * @returns {string}
   *          The GUID of the newly added item..
   */
  add(record, {sourceSync = false} = {}) {
    this.log.debug("add:", record);

    let recordToSave = this._clone(record);

    if (sourceSync) {
      // Remove tombstones for incoming items that were changed on another
      // device. Local deletions always lose to avoid data loss.
      let index = this._findIndexByGUID(recordToSave.guid, {
        includeDeleted: true,
      });
      if (index > -1) {
        let existing = this._data[index];
        if (existing.deleted) {
          this._data.splice(index, 1);
        } else {
          throw new Error(`Record ${recordToSave.guid} already exists`);
        }
      }
    } else if (!recordToSave.deleted) {
      this._normalizeRecord(recordToSave);

      recordToSave.guid = this._generateGUID();
      recordToSave.version = this.version;

      // Metadata
      let now = Date.now();
      recordToSave.timeCreated = now;
      recordToSave.timeLastModified = now;
      recordToSave.timeLastUsed = 0;
      recordToSave.timesUsed = 0;
    }

    return this._saveRecord(recordToSave, {sourceSync});
  }

  _saveRecord(record, {sourceSync = false} = {}) {
    if (!record.guid) {
      throw new Error("Record missing GUID");
    }

    let recordToSave;
    if (record.deleted) {
      if (this._findByGUID(record.guid, {includeDeleted: true})) {
        throw new Error("a record with this GUID already exists");
      }
      recordToSave = {
        guid: record.guid,
        timeLastModified: record.timeLastModified || Date.now(),
        deleted: true,
      };
    } else {
      this._ensureMatchingVersion(record);
      recordToSave = record;
      this._computeFields(recordToSave);
    }

    if (sourceSync) {
      let sync = this._getSyncMetaData(recordToSave, true);
      sync.changeCounter = 0;
    }

    this._data.push(recordToSave);

    this._store.saveSoon();

    Services.obs.notifyObservers({wrappedJSObject: {
      sourceSync,
      guid: record.guid,
      collectionName: this._collectionName,
    }}, "formautofill-storage-changed", "add");
    return recordToSave.guid;
  }

  _generateGUID() {
    let guid;
    while (!guid || this._findByGUID(guid)) {
      guid = gUUIDGenerator.generateUUID().toString()
                           .replace(/[{}-]/g, "").substring(0, 12);
    }
    return guid;
  }

  /**
   * Update the specified record.
   *
   * @param  {string} guid
   *         Indicates which record to update.
   * @param  {Object} record
   *         The new record used to overwrite the old one.
   * @param  {boolean} [preserveOldProperties = false]
   *         Preserve old record's properties if they don't exist in new record.
   */
  update(guid, record, preserveOldProperties = false) {
    this.log.debug("update:", guid, record);

    let recordFoundIndex = this._findIndexByGUID(guid);
    if (recordFoundIndex == -1) {
      throw new Error("No matching record.");
    }

    // Clone the record before modifying it to avoid exposing incomplete changes.
    let recordFound = this._clone(this._data[recordFoundIndex]);
    this._stripComputedFields(recordFound);

    let recordToUpdate = this._clone(record);
    this._normalizeRecord(recordToUpdate, true);

    let hasValidField = false;
    for (let field of this.VALID_FIELDS) {
      let oldValue = recordFound[field];
      let newValue = recordToUpdate[field];

      // Resume the old field value in the perserve case
      if (preserveOldProperties && newValue === undefined) {
        newValue = oldValue;
      }

      if (newValue === undefined || newValue === "") {
        delete recordFound[field];
      } else {
        hasValidField = true;
        recordFound[field] = newValue;
      }

      this._maybeStoreLastSyncedField(recordFound, field, oldValue);
    }

    if (!hasValidField) {
      throw new Error("Record contains no valid field.");
    }

    recordFound.timeLastModified = Date.now();
    let syncMetadata = this._getSyncMetaData(recordFound);
    if (syncMetadata) {
      syncMetadata.changeCounter += 1;
    }

    this._computeFields(recordFound);
    this._data[recordFoundIndex] = recordFound;

    this._store.saveSoon();

    Services.obs.notifyObservers({wrappedJSObject: {
      guid,
      collectionName: this._collectionName,
    }}, "formautofill-storage-changed", "update");
  }

  /**
   * Notifies the storage of the use of the specified record, so we can update
   * the metadata accordingly. This does not bump the Sync change counter, since
   * we don't sync `timesUsed` or `timeLastUsed`.
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
    Services.obs.notifyObservers({wrappedJSObject: {
      guid,
      collectionName: this._collectionName,
    }}, "formautofill-storage-changed", "notifyUsed");
  }

  /**
   * Removes the specified record. No error occurs if the record isn't found.
   *
   * @param  {string} guid
   *         Indicates which record to remove.
   * @param  {boolean} [options.sourceSync = false]
   *         Did Sync generate this removal?
   */
  remove(guid, {sourceSync = false} = {}) {
    this.log.debug("remove:", guid);

    if (sourceSync) {
      this._removeSyncedRecord(guid);
    } else {
      let index = this._findIndexByGUID(guid, {includeDeleted: false});
      if (index == -1) {
        this.log.warn("attempting to remove non-existing entry", guid);
        return;
      }
      let existing = this._data[index];
      if (existing.deleted) {
        return; // already a tombstone - don't touch it.
      }
      let existingSync = this._getSyncMetaData(existing);
      if (existingSync) {
        // existing sync metadata means it has been synced. This means we must
        // leave a tombstone behind.
        this._data[index] = {
          guid,
          timeLastModified: Date.now(),
          deleted: true,
          _sync: existingSync,
        };
        existingSync.changeCounter++;
      } else {
        // If there's no sync meta-data, this record has never been synced, so
        // we can delete it.
        this._data.splice(index, 1);
      }
    }

    this._store.saveSoon();
    Services.obs.notifyObservers({wrappedJSObject: {
      sourceSync,
      guid,
      collectionName: this._collectionName,
    }}, "formautofill-storage-changed", "remove");
  }

  /**
   * Returns the record with the specified GUID.
   *
   * @param   {string} guid
   *          Indicates which record to retrieve.
   * @param   {boolean} [options.rawData = false]
   *          Returns a raw record without modifications and the computed fields
   *          (this includes private fields)
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
    let clonedRecord = this._cloneAndCleanUp(recordFound);
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

    let records = this._data.filter(r => !r.deleted || includeDeleted);
    // Records are cloned to avoid accidental modifications from outside.
    let clonedRecords = records.map(r => this._cloneAndCleanUp(r));
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
   * Functions intended to be used in the support of Sync.
   */

  /**
   * Stores a hash of the last synced value for a field in a locally updated
   * record. We use this value to rebuild the shared parent, or base, when
   * reconciling incoming records that may have changed on another device.
   *
   * Storing the hash of the values that we last wrote to the Sync server lets
   * us determine if a remote change conflicts with a local change. If the
   * hashes for the base, current local value, and remote value all differ, we
   * have a conflict.
   *
   * These fields are not themselves synced, and will be removed locally as
   * soon as we have successfully written the record to the Sync server - so
   * it is expected they will not remain for long, as changes which cause a
   * last synced field to be written will itself cause a sync.
   *
   * We also skip this for updates made by Sync, for internal fields, for
   * records that haven't been uploaded yet, and for fields which have already
   * been changed since the last sync.
   *
   * @param   {Object} record
   *          The updated local record.
   * @param   {string} field
   *          The field name.
   * @param   {string} lastSyncedValue
   *          The last synced field value.
   */
  _maybeStoreLastSyncedField(record, field, lastSyncedValue) {
    let sync = this._getSyncMetaData(record);
    if (!sync) {
      // The record hasn't been uploaded yet, so we can't end up with merge
      // conflicts.
      return;
    }
    let alreadyChanged = field in sync.lastSyncedFields;
    if (alreadyChanged) {
      // This field was already changed multiple times since the last sync.
      return;
    }
    let newValue = record[field];
    if (lastSyncedValue != newValue) {
      sync.lastSyncedFields[field] = sha512(lastSyncedValue);
    }
  }

  /**
   * Attempts a three-way merge between a changed local record, an incoming
   * remote record, and the shared parent that we synthesize from the last
   * synced fields - see _maybeStoreLastSyncedField.
   *
   * @param   {Object} strippedLocalRecord
   *          The changed local record, currently in storage. Computed fields
   *          are stripped.
   * @param   {Object} remoteRecord
   *          The remote record.
   * @returns {Object|null}
   *          The merged record, or `null` if there are conflicts and the
   *          records can't be merged.
   */
  _mergeSyncedRecords(strippedLocalRecord, remoteRecord) {
    let sync = this._getSyncMetaData(strippedLocalRecord, true);

    // Copy all internal fields from the remote record. We'll update their
    // values in `_replaceRecordAt`.
    let mergedRecord = {};
    for (let field of INTERNAL_FIELDS) {
      if (remoteRecord[field] != null) {
        mergedRecord[field] = remoteRecord[field];
      }
    }

    for (let field of this.VALID_FIELDS) {
      let isLocalSame = false;
      let isRemoteSame = false;
      if (field in sync.lastSyncedFields) {
        // If the field has changed since the last sync, compare hashes to
        // determine if the local and remote values are different. Hashing is
        // expensive, but we don't expect this to happen frequently.
        let lastSyncedValue = sync.lastSyncedFields[field];
        isLocalSame = lastSyncedValue == sha512(strippedLocalRecord[field]);
        isRemoteSame = lastSyncedValue == sha512(remoteRecord[field]);
      } else {
        // Otherwise, if the field hasn't changed since the last sync, we know
        // it's the same locally.
        isLocalSame = true;
        isRemoteSame = strippedLocalRecord[field] == remoteRecord[field];
      }

      let value;
      if (isLocalSame && isRemoteSame) {
        // Local and remote are the same; doesn't matter which one we pick.
        value = strippedLocalRecord[field];
      } else if (isLocalSame && !isRemoteSame) {
        value = remoteRecord[field];
      } else if (!isLocalSame && isRemoteSame) {
        // We don't need to bump the change counter when taking the local
        // change, because the counter must already be > 0 if we're attempting
        // a three-way merge.
        value = strippedLocalRecord[field];
      } else if (strippedLocalRecord[field] == remoteRecord[field]) {
        // Shared parent doesn't match either local or remote, but the values
        // are identical, so there's no conflict.
        value = strippedLocalRecord[field];
      } else {
        // Both local and remote changed to different values. We'll need to fork
        // the local record to resolve the conflict.
        return null;
      }

      if (value != null) {
        mergedRecord[field] = value;
      }
    }

    return mergedRecord;
  }

  /**
   * Replaces a local record with a remote or merged record, copying internal
   * fields and Sync metadata.
   *
   * @param   {number} index
   * @param   {Object} remoteRecord
   * @param   {boolean} [options.keepSyncMetadata = false]
   *          Should we copy Sync metadata? This is true if `remoteRecord` is a
   *          merged record with local changes that we need to upload. Passing
   *          `keepSyncMetadata` retains the record's change counter and
   *          last synced fields, so that we don't clobber the local change if
   *          the sync is interrupted after the record is merged, but before
   *          it's uploaded.
   */
  _replaceRecordAt(index, remoteRecord, {keepSyncMetadata = false} = {}) {
    let localRecord = this._data[index];
    let newRecord = this._clone(remoteRecord);

    this._stripComputedFields(newRecord);

    this._data[index] = newRecord;

    if (keepSyncMetadata) {
      // It's safe to move the Sync metadata from the old record to the new
      // record, since we always clone records when we return them, and we
      // never hand out references to the metadata object via public methods.
      newRecord._sync = localRecord._sync;
    } else {
      // As a side effect, `_getSyncMetaData` marks the record as syncing if the
      // existing `localRecord` is a dupe of `remoteRecord`, and we're replacing
      // local with remote.
      let sync = this._getSyncMetaData(newRecord, true);
      sync.changeCounter = 0;
    }

    if (!newRecord.timeCreated ||
        localRecord.timeCreated < newRecord.timeCreated) {
      newRecord.timeCreated = localRecord.timeCreated;
    }

    if (!newRecord.timeLastModified ||
        localRecord.timeLastModified > newRecord.timeLastModified) {
      newRecord.timeLastModified = localRecord.timeLastModified;
    }

    // Copy local-only fields from the existing local record.
    for (let field of ["timeLastUsed", "timesUsed"]) {
      if (localRecord[field] != null) {
        newRecord[field] = localRecord[field];
      }
    }

    this._computeFields(newRecord);
  }

  /**
   * Clones a local record, giving the clone a new GUID and Sync metadata. The
   * original record remains unchanged in storage.
   *
   * @param   {Object} strippedLocalRecord
   *          The local record. Computed fields are stripped.
   * @returns {string}
   *          A clone of the local record with a new GUID.
   */
  _forkLocalRecord(strippedLocalRecord) {
    let forkedLocalRecord = this._cloneAndCleanUp(strippedLocalRecord);
    forkedLocalRecord.guid = this._generateGUID();

    // Give the record fresh Sync metadata and bump its change counter as a
    // side effect. This also excludes the forked record from de-duping on the
    // next sync, if the current sync is interrupted before the record can be
    // uploaded.
    this._getSyncMetaData(forkedLocalRecord, true);

    this._computeFields(forkedLocalRecord);
    this._data.push(forkedLocalRecord);

    return forkedLocalRecord;
  }

  /**
   * Reconciles an incoming remote record into the matching local record. This
   * method is only used by Sync; other callers should use `merge`.
   *
   * @param   {Object} remoteRecord
   *          The incoming record. `remoteRecord` must not be a tombstone, and
   *          must have a matching local record with the same GUID. Use
   *          `add` to insert remote records that don't exist locally, and
   *          `remove` to apply remote tombstones.
   * @returns {Object}
   *          A `{forkedGUID}` tuple. `forkedGUID` is `null` if the merge
   *          succeeded without conflicts, or a new GUID referencing the
   *          existing locally modified record if the conflicts could not be
   *          resolved.
   */
  reconcile(remoteRecord) {
    this._ensureMatchingVersion(remoteRecord);
    if (remoteRecord.deleted) {
      throw new Error(`Can't reconcile tombstone ${remoteRecord.guid}`);
    }

    let localIndex = this._findIndexByGUID(remoteRecord.guid);
    if (localIndex < 0) {
      throw new Error(`Record ${remoteRecord.guid} not found`);
    }

    let localRecord = this._data[localIndex];
    let sync = this._getSyncMetaData(localRecord, true);

    let forkedGUID = null;

    if (sync.changeCounter === 0) {
      // Local not modified. Replace local with remote.
      this._replaceRecordAt(localIndex, remoteRecord, {
        keepSyncMetadata: false,
      });
    } else {
      let strippedLocalRecord = this._clone(localRecord);
      this._stripComputedFields(strippedLocalRecord);

      let mergedRecord = this._mergeSyncedRecords(strippedLocalRecord, remoteRecord);
      if (mergedRecord) {
        // Local and remote modified, but we were able to merge. Replace the
        // local record with the merged record.
        this._replaceRecordAt(localIndex, mergedRecord, {
          keepSyncMetadata: true,
        });
      } else {
        // Merge conflict. Fork the local record, then replace the original
        // with the merged record.
        let forkedLocalRecord = this._forkLocalRecord(strippedLocalRecord);
        forkedGUID = forkedLocalRecord.guid;
        this._replaceRecordAt(localIndex, remoteRecord, {
          keepSyncMetadata: false,
        });
      }
    }

    this._store.saveSoon();
    Services.obs.notifyObservers({wrappedJSObject: {
      sourceSync: true,
      guid: remoteRecord.guid,
      forkedGUID,
      collectionName: this._collectionName,
    }}, "formautofill-storage-changed", "reconcile");

    return {forkedGUID};
  }

  _removeSyncedRecord(guid) {
    let index = this._findIndexByGUID(guid, {includeDeleted: true});
    if (index == -1) {
      // Removing a record we don't know about. It may have been synced and
      // removed by another device before we saw it. Store the tombstone in
      // case the server is later wiped and we need to reupload everything.
      let tombstone = {
        guid,
        timeLastModified: Date.now(),
        deleted: true,
      };

      let sync = this._getSyncMetaData(tombstone, true);
      sync.changeCounter = 0;
      this._data.push(tombstone);
      return;
    }

    let existing = this._data[index];
    let sync = this._getSyncMetaData(existing, true);
    if (sync.changeCounter > 0) {
      // Deleting a record with unsynced local changes. To avoid potential
      // data loss, we ignore the deletion in favor of the changed record.
      this.log.info("Ignoring deletion for record with local changes",
                    existing);
      return;
    }

    if (existing.deleted) {
      this.log.info("Ignoring deletion for tombstone", existing);
      return;
    }

    // Removing a record that's not changed locally, and that's not already
    // deleted. Replace the record with a synced tombstone.
    this._data[index] = {
      guid,
      timeLastModified: Date.now(),
      deleted: true,
      _sync: sync,
    };
  }

  /**
   * Provide an object that describes the changes to sync.
   *
   * This is called at the start of the sync process to determine what needs
   * to be updated on the server. As the server is updated, sync will update
   * entries in the returned object, and when sync is complete it will pass
   * the object to pushSyncChanges, which will apply the changes to the store.
   *
   * @returns {object}
   *          An object describing the changes to sync.
   */
  pullSyncChanges() {
    let changes = {};

    let profiles = this._data;
    for (let profile of profiles) {
      let sync = this._getSyncMetaData(profile, true);
      if (sync.changeCounter < 1) {
        if (sync.changeCounter != 0) {
          this.log.error("negative change counter", profile);
        }
        continue;
      }
      changes[profile.guid] = {
        profile,
        counter: sync.changeCounter,
        modified: profile.timeLastModified,
        synced: false,
      };
    }
    this._store.saveSoon();

    return changes;
  }

  /**
   * Apply the metadata changes made by Sync.
   *
   * This is called with metadata about what was synced - see pullSyncChanges.
   *
   * @param {object} changes
   *        The possibly modified object obtained via pullSyncChanges.
   */
  pushSyncChanges(changes) {
    for (let [guid, {counter, synced}] of Object.entries(changes)) {
      if (!synced) {
        continue;
      }
      let recordFound = this._findByGUID(guid, {includeDeleted: true});
      if (!recordFound) {
        this.log.warn("No profile found to persist changes for guid " + guid);
        continue;
      }
      let sync = this._getSyncMetaData(recordFound, true);
      sync.changeCounter = Math.max(0, sync.changeCounter - counter);
      if (sync.changeCounter === 0) {
        // Clear the shared parent fields once we've uploaded all pending
        // changes, since the server now matches what we have locally.
        sync.lastSyncedFields = {};
      }
    }
    this._store.saveSoon();
  }

  /**
   * Reset all sync metadata for all items.
   *
   * This is called when Sync is disconnected from this device. All sync
   * metadata for all items is removed.
   */
  resetSync() {
    for (let record of this._data) {
      delete record._sync;
    }
    // XXX - we should probably also delete all tombstones?
    this.log.info("All sync metadata was reset");
  }

  /**
   * Changes the GUID of an item. This should be called only by Sync. There
   * must be an existing record with oldID and it must never have been synced
   * or an error will be thrown. There must be no existing record with newID.
   *
   * No tombstone will be created for the old GUID - we check it hasn't
   * been synced, so no tombstone is necessary.
   *
   * @param   {string} oldID
   *          GUID of the existing item to change the GUID of.
   * @param   {string} newID
   *          The new GUID for the item.
   */
  changeGUID(oldID, newID) {
    this.log.debug("changeGUID: ", oldID, newID);
    if (oldID == newID) {
      throw new Error("changeGUID: old and new IDs are the same");
    }
    if (this._findIndexByGUID(newID) >= 0) {
      throw new Error("changeGUID: record with destination id exists already");
    }

    let index = this._findIndexByGUID(oldID);
    let profile = this._data[index];
    if (!profile) {
      throw new Error("changeGUID: no source record");
    }
    if (this._getSyncMetaData(profile)) {
      throw new Error("changeGUID: existing record has already been synced");
    }

    profile.guid = newID;

    this._store.saveSoon();
  }

  // Used to get, and optionally create, sync metadata. Brand new records will
  // *not* have sync meta-data - it will be created when they are first
  // synced.
  _getSyncMetaData(record, forceCreate = false) {
    if (!record._sync && forceCreate) {
      // create default metadata and indicate we need to save.
      record._sync = {
        changeCounter: 1,
        lastSyncedFields: {},
      };
      this._store.saveSoon();
    }
    return record._sync;
  }

  /**
   * Finds a local record with matching common fields and a different GUID.
   * Sync uses this method to find and update unsynced local records with
   * fields that match incoming remote records. This avoids creating
   * duplicate profiles with the same information.
   *
   * @param   {Object} remoteRecord
   *          The remote record.
   * @returns {string|null}
   *          The GUID of the matching local record, or `null` if no records
   *          match.
   */
  findDuplicateGUID(remoteRecord) {
    if (!remoteRecord.guid) {
      throw new Error("Record missing GUID");
    }
    this._ensureMatchingVersion(remoteRecord);
    if (remoteRecord.deleted) {
      // Tombstones don't carry enough info to de-dupe, and we should have
      // handled them separately when applying the record.
      throw new Error("Tombstones can't have duplicates");
    }
    let localRecords = this._data;
    for (let localRecord of localRecords) {
      if (localRecord.deleted) {
        continue;
      }
      if (localRecord.guid == remoteRecord.guid) {
        throw new Error(`Record ${remoteRecord.guid} already exists`);
      }
      if (this._getSyncMetaData(localRecord)) {
        // This local record has already been uploaded, so it can't be a dupe of
        // another incoming item.
        continue;
      }

      // Ignore computed fields when matching records as they aren't synced at all.
      let strippedLocalRecord = this._clone(localRecord);
      this._stripComputedFields(strippedLocalRecord);

      let keys = new Set(Object.keys(remoteRecord));
      for (let key of Object.keys(strippedLocalRecord)) {
        keys.add(key);
      }
      // Ignore internal fields when matching records. Internal fields are synced,
      // but almost certainly have different values than the local record, and
      // we'll update them in `reconcile`.
      for (let field of INTERNAL_FIELDS) {
        keys.delete(field);
      }
      if (!keys.size) {
        // This shouldn't ever happen; a valid record will always have fields
        // that aren't computed or internal. Sync can't do anything about that,
        // so we ignore the dubious local record instead of throwing.
        continue;
      }
      let same = true;
      for (let key of keys) {
        // For now, we ensure that both (or neither) records have the field
        // with matching values. This doesn't account for the version yet
        // (bug 1377204).
        same = key in strippedLocalRecord == key in remoteRecord && strippedLocalRecord[key] == remoteRecord[key];
        if (!same) {
          break;
        }
      }
      if (same) {
        return strippedLocalRecord.guid;
      }
    }
    return null;
  }

  /**
   * Internal helper functions.
   */

  _clone(record) {
    return Object.assign({}, record);
  }

  _cloneAndCleanUp(record) {
    let result = {};
    for (let key in record) {
      // Do not expose hidden fields and fields with empty value (mainly used
      // as placeholders of the computed fields).
      if (!key.startsWith("_") && record[key] !== "") {
        result[key] = record[key];
      }
    }
    return result;
  }

  _findByGUID(guid, {includeDeleted = false} = {}) {
    let found = this._findIndexByGUID(guid, {includeDeleted});
    return found < 0 ? undefined : this._data[found];
  }

  _findIndexByGUID(guid, {includeDeleted = false} = {}) {
    return this._data.findIndex(record => {
      return record.guid == guid && (!record.deleted || includeDeleted);
    });
  }

  _migrateRecord(record) {
    let hasChanges = false;

    if (record.deleted) {
      return hasChanges;
    }

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

  _normalizeRecord(record, preserveEmptyFields = false) {
    this._normalizeFields(record);

    for (let key in record) {
      if (!this.VALID_FIELDS.includes(key)) {
        throw new Error(`"${key}" is not a valid field.`);
      }
      if (typeof record[key] !== "string" &&
          typeof record[key] !== "number") {
        throw new Error(`"${key}" contains invalid data type.`);
      }
      if (!preserveEmptyFields && record[key] === "") {
        delete record[key];
      }
    }

    if (!Object.keys(record).length) {
      throw new Error("Record contains no valid field.");
    }
  }

  /**
   * Merge the record if storage has multiple mergeable records.
   * @param {Object} targetRecord
   *        The record for merge.
   * @param {boolean} [strict = false]
   *        In strict merge mode, we'll treat the subset record with empty field
   *        as unable to be merged, but mergeable if in non-strict mode.
   * @returns {Array.<string>}
   *          Return an array of the merged GUID string.
   */
  mergeToStorage(targetRecord, strict = false) {
    let mergedGUIDs = [];
    for (let record of this._data) {
      if (!record.deleted && this.mergeIfPossible(record.guid, targetRecord, strict)) {
        mergedGUIDs.push(record.guid);
      }
    }
    this.log.debug("Existing records matching and merging count is", mergedGUIDs.length);
    return mergedGUIDs;
  }

  /**
   * Unconditionally remove all data and tombstones for this collection.
   */
  removeAll({sourceSync = false} = {}) {
    this._store.data[this._collectionName] = [];
    this._store.saveSoon();
    Services.obs.notifyObservers({wrappedJSObject: {
      sourceSync,
      collectionName: this._collectionName,
    }}, "formautofill-storage-changed", "removeAll");
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
  mergeIfPossible(guid, record, strict) {}
}

class Addresses extends AutofillRecords {
  constructor(store) {
    super(store, "addresses", VALID_ADDRESS_FIELDS, VALID_ADDRESS_COMPUTED_FIELDS, ADDRESS_SCHEMA_VERSION);
  }

  _recordReadProcessor(address) {
    if (address.country && !FormAutofillUtils.supportedCountries.includes(address.country)) {
      delete address.country;
      delete address["country-name"];
    }
  }

  _computeFields(address) {
    // NOTE: Remember to bump the schema version number if any of the existing
    //       computing algorithm changes. (No need to bump when just adding new
    //       computed fields.)

    // NOTE: Computed fields should be always present in the storage no matter
    //       it's empty or not.

    let hasNewComputedFields = false;

    if (address.deleted) {
      return hasNewComputedFields;
    }

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
      if (address.country) {
        try {
          address["country-name"] = Services.intl.getRegionDisplayNames(undefined, [address.country]);
        } catch (e) {
          address["country-name"] = "";
        }
      } else {
        address["country-name"] = "";
      }
      hasNewComputedFields = true;
    }

    // Compute tel
    if (!("tel-national" in address)) {
      if (address.tel) {
        let tel = PhoneNumber.Parse(address.tel, address.country || FormAutofillUtils.DEFAULT_REGION);
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
    if (address.name) {
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
    }
    delete address.name;
  }

  _normalizeAddress(address) {
    if (STREET_ADDRESS_COMPONENTS.some(c => !!address[c])) {
      // Treat "street-address" as "address-line1" if it contains only one line
      // and "address-line1" is omitted.
      if (!address["address-line1"] && address["street-address"] &&
          !address["street-address"].includes("\n")) {
        address["address-line1"] = address["street-address"];
        delete address["street-address"];
      }

      // Concatenate "address-line*" if "street-address" is omitted.
      if (!address["street-address"]) {
        address["street-address"] = STREET_ADDRESS_COMPONENTS.map(c => address[c]).join("\n").replace(/\n+$/, "");
      }
    }
    STREET_ADDRESS_COMPONENTS.forEach(c => delete address[c]);
  }

  _normalizeCountry(address) {
    let country;

    if (address.country) {
      country = address.country.toUpperCase();
    } else if (address["country-name"]) {
      country = FormAutofillUtils.identifyCountryCode(address["country-name"]);
    }

    // Only values included in the region list will be saved.
    let hasLocalizedName = false;
    try {
      let localizedName = Services.intl.getRegionDisplayNames(undefined, [country]);
      hasLocalizedName = localizedName != country;
    } catch (e) {}

    if (country && hasLocalizedName) {
      address.country = country;
    } else {
      delete address.country;
    }

    delete address["country-name"];
  }

  _normalizeTel(address) {
    if (address.tel || TEL_COMPONENTS.some(c => !!address[c])) {
      FormAutofillUtils.compressTel(address);

      let possibleRegion = address.country || FormAutofillUtils.DEFAULT_REGION;
      let tel = PhoneNumber.Parse(address.tel, possibleRegion);

      if (tel && tel.internationalNumber) {
        // Force to save numbers in E.164 format if parse success.
        address.tel = tel.internationalNumber;
      }
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
   * @param  {boolean} strict
   *         In strict merge mode, we'll treat the subset record with empty field
   *         as unable to be merged, but mergeable if in non-strict mode.
   * @returns {boolean}
   *          Return true if address is merged into target with specific guid or false if not.
   */
  mergeIfPossible(guid, address, strict) {
    this.log.debug("mergeIfPossible:", guid, address);

    let addressFound = this._findByGUID(guid);
    if (!addressFound) {
      throw new Error("No matching address.");
    }

    let addressToMerge = this._clone(address);
    this._normalizeRecord(addressToMerge, strict);
    let hasMatchingField = false;

    for (let field of this.VALID_FIELDS) {
      let existingField = addressFound[field];
      let incomingField = addressToMerge[field];
      if (incomingField !== undefined && existingField !== undefined) {
        if (incomingField != existingField) {
          // Treat "street-address" as mergeable if their single-line versions
          // match each other.
          if (field == "street-address" &&
              FormAutofillUtils.toOneLineAddress(existingField) == FormAutofillUtils.toOneLineAddress(incomingField)) {
            // Keep the value in storage if its amount of lines is greater than
            // or equal to the incoming one.
            if (existingField.split("\n").length >= incomingField.split("\n").length) {
              // Replace the incoming field with the one in storage so it will
              // be further merged back to storage.
              addressToMerge[field] = existingField;
            }
          } else {
            this.log.debug("Conflicts: field", field, "has different value.");
            return false;
          }
        }
        hasMatchingField = true;
      }
    }

    // We merge the address only when at least one field has the same value.
    if (!hasMatchingField) {
      this.log.debug("Unable to merge because no field has the same value");
      return false;
    }

    // Early return if the data is the same or subset.
    let noNeedToUpdate = this.VALID_FIELDS.every((field) => {
      // When addressFound doesn't contain a field, it's unnecessary to update
      // if the same field in addressToMerge is omitted or an empty string.
      if (addressFound[field] === undefined) {
        return !addressToMerge[field];
      }

      // When addressFound contains a field, it's unnecessary to update if
      // the same field in addressToMerge is omitted or a duplicate.
      return (addressToMerge[field] === undefined) ||
             (addressFound[field] === addressToMerge[field]);
    });
    if (noNeedToUpdate) {
      return true;
    }

    this.update(guid, addressToMerge, true);
    return true;
  }
}

class CreditCards extends AutofillRecords {
  constructor(store) {
    super(store, "creditCards", VALID_CREDIT_CARD_FIELDS, VALID_CREDIT_CARD_COMPUTED_FIELDS, CREDIT_CARD_SCHEMA_VERSION);
  }

  _getMaskedCCNumber(ccNumber) {
    if (ccNumber.length <= 4) {
      throw new Error(`Invalid credit card number`);
    }
    return "*".repeat(ccNumber.length - 4) + ccNumber.substr(-4);
  }

  _computeFields(creditCard) {
    // NOTE: Remember to bump the schema version number if any of the existing
    //       computing algorithm changes. (No need to bump when just adding new
    //       computed fields.)

    // NOTE: Computed fields should be always present in the storage no matter
    //       it's empty or not.

    let hasNewComputedFields = false;

    if (creditCard.deleted) {
      return hasNewComputedFields;
    }

    // Compute split names
    if (!("cc-given-name" in creditCard)) {
      let nameParts = FormAutofillNameUtils.splitName(creditCard["cc-name"]);
      creditCard["cc-given-name"] = nameParts.given;
      creditCard["cc-additional-name"] = nameParts.middle;
      creditCard["cc-family-name"] = nameParts.family;
      hasNewComputedFields = true;
    }

    // Compute credit card expiration date
    if (!("cc-exp" in creditCard)) {
      if (creditCard["cc-exp-month"] && creditCard["cc-exp-year"]) {
        creditCard["cc-exp"] = String(creditCard["cc-exp-year"]) + "-" + String(creditCard["cc-exp-month"]).padStart(2, "0");
      } else {
        creditCard["cc-exp"] = "";
      }
      hasNewComputedFields = true;
    }

    // Encrypt credit card number
    if (!("cc-number-encrypted" in creditCard)) {
      if ("cc-number" in creditCard) {
        let ccNumber = creditCard["cc-number"];
        creditCard["cc-number"] = this._getMaskedCCNumber(ccNumber);
        creditCard["cc-number-encrypted"] = MasterPassword.encryptSync(ccNumber);
      } else {
        creditCard["cc-number-encrypted"] = "";
      }
    }

    return hasNewComputedFields;
  }

  _stripComputedFields(creditCard) {
    if (creditCard["cc-number-encrypted"]) {
      creditCard["cc-number"] = MasterPassword.decryptSync(creditCard["cc-number-encrypted"]);
    }
    super._stripComputedFields(creditCard);
  }

  _normalizeFields(creditCard) {
    this._normalizeCCName(creditCard);
    this._normalizeCCNumber(creditCard);
    this._normalizeCCExpirationDate(creditCard);
  }

  _normalizeCCName(creditCard) {
    if (creditCard["cc-given-name"] || creditCard["cc-additional-name"] || creditCard["cc-family-name"]) {
      if (!creditCard["cc-name"]) {
        creditCard["cc-name"] = FormAutofillNameUtils.joinNameParts({
          given: creditCard["cc-given-name"],
          middle: creditCard["cc-additional-name"],
          family: creditCard["cc-family-name"],
        });
      }
    }
    delete creditCard["cc-given-name"];
    delete creditCard["cc-additional-name"];
    delete creditCard["cc-family-name"];
  }

  _normalizeCCNumber(creditCard) {
    if (creditCard["cc-number"]) {
      creditCard["cc-number"] = FormAutofillUtils.normalizeCCNumber(creditCard["cc-number"]);
      if (!creditCard["cc-number"]) {
        delete creditCard["cc-number"];
      }
    }
  }

  _normalizeCCExpirationDate(creditCard) {
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

    if (creditCard["cc-exp"] && (!creditCard["cc-exp-month"] || !creditCard["cc-exp-year"])) {
      let rules = [
        {
          regex: "(\\d{4})[-/](\\d{1,2})",
          yearIndex: 1,
          monthIndex: 2,
        },
        {
          regex: "(\\d{1,2})[-/](\\d{4})",
          yearIndex: 2,
          monthIndex: 1,
        },
        {
          regex: "(\\d{1,2})[-/](\\d{1,2})",
        },
        {
          regex: "(\\d{2})(\\d{2})",
        },
      ];

      for (let rule of rules) {
        let result = new RegExp(`(?:^|\\D)${rule.regex}(?!\\d)`).exec(creditCard["cc-exp"]);
        if (!result) {
          continue;
        }

        let expYear, expMonth;

        if (!rule.yearIndex || !rule.monthIndex) {
          expMonth = parseInt(result[1], 10);
          if (expMonth > 12) {
            expYear = parseInt(result[1], 10);
            expMonth = parseInt(result[2], 10);
          } else {
            expYear = parseInt(result[2], 10);
          }
        } else {
          expYear = parseInt(result[rule.yearIndex], 10);
          expMonth = parseInt(result[rule.monthIndex], 10);
        }

        if (expMonth < 1 || expMonth > 12) {
          continue;
        }

        if (expYear < 100) {
          expYear += 2000;
        } else if (expYear < 2000) {
          continue;
        }

        creditCard["cc-exp-month"] = expMonth;
        creditCard["cc-exp-year"] = expYear;
        break;
      }
    }

    delete creditCard["cc-exp"];
  }

  /**
   * Normalize the given record and return the first matched guid if storage has the same record.
   * @param {Object} targetCreditCard
   *        The credit card for duplication checking.
   * @returns {string|null}
   *          Return the first guid if storage has the same credit card and null otherwise.
   */
  getDuplicateGuid(targetCreditCard) {
    let clonedTargetCreditCard = this._clone(targetCreditCard);
    this._normalizeRecord(clonedTargetCreditCard);
    for (let creditCard of this._data) {
      let isDuplicate = this.VALID_FIELDS.every(field => {
        if (!clonedTargetCreditCard[field]) {
          return !creditCard[field];
        }
        if (field == "cc-number" && creditCard[field]) {
          if (MasterPassword.isEnabled) {
            // Compare the masked numbers instead when the master password is
            // enabled because we don't want to leak the credit card number.
            return this._getMaskedCCNumber(clonedTargetCreditCard[field]) == creditCard[field];
          }
          return clonedTargetCreditCard[field] == MasterPassword.decryptSync(creditCard["cc-number-encrypted"]);
        }
        return clonedTargetCreditCard[field] == creditCard[field];
      });
      if (isDuplicate) {
        return creditCard.guid;
      }
    }
    return null;
  }

  /**
   * Merge new credit card into the specified record if cc-number is identical.
   * (Note that credit card records always do non-strict merge.)
   *
   * @param  {string} guid
   *         Indicates which credit card to merge.
   * @param  {Object} creditCard
   *         The new credit card used to merge into the old one.
   * @returns {boolean}
   *          Return true if credit card is merged into target with specific guid or false if not.
   */
  mergeIfPossible(guid, creditCard) {
    this.log.debug("mergeIfPossible:", guid, creditCard);

    // Query raw data for comparing the decrypted credit card number
    let creditCardFound = this.get(guid, {rawData: true});
    if (!creditCardFound) {
      throw new Error("No matching credit card.");
    }

    let creditCardToMerge = this._clone(creditCard);
    this._normalizeRecord(creditCardToMerge);

    for (let field of this.VALID_FIELDS) {
      let existingField = creditCardFound[field];

      // Make sure credit card field is existed and have value
      if (field == "cc-number" && (!existingField || !creditCardToMerge[field])) {
        return false;
      }

      if (!creditCardToMerge[field] && typeof(existingField) != "undefined") {
        creditCardToMerge[field] = existingField;
      }

      let incomingField = creditCardToMerge[field];
      if (incomingField && existingField) {
        if (incomingField != existingField) {
          this.log.debug("Conflicts: field", field, "has different value.");
          return false;
        }
      }
    }

    // Early return if the data is the same.
    let exactlyMatch = this.VALID_FIELDS.every((field) =>
      creditCardFound[field] === creditCardToMerge[field]
    );
    if (exactlyMatch) {
      return true;
    }

    this.update(guid, creditCardToMerge, true);
    return true;
  }
}

function FormAutofillStorage(path) {
  this._path = path;
  this._initializePromise = null;
  this.INTERNAL_FIELDS = INTERNAL_FIELDS;
}

FormAutofillStorage.prototype = {
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
this.formAutofillStorage = new FormAutofillStorage(
  OS.Path.join(OS.Constants.Path.profileDir, PROFILE_JSON_FILE_NAME));
