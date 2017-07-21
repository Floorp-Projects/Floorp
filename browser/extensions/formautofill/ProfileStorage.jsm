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
 *       _sync: { ... optional sync metadata },
 *     }
 *   ]
 * }
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

    if (sourceSync) {
      // Remove tombstones for incoming items that were changed on another
      // device. Local deletions always lose to avoid data loss.
      let index = this._findIndexByGUID(record.guid, {
        includeDeleted: true,
      });
      if (index > -1) {
        let existing = this._store.data[this._collectionName][index];
        if (existing.deleted) {
          this._store.data[this._collectionName].splice(index, 1);
        } else {
          throw new Error(`Record ${record.guid} already exists`);
        }
      }
      let recordToSave = this._clone(record);
      return this._saveRecord(recordToSave, {sourceSync});
    }

    if (record.deleted) {
      return this._saveRecord(record);
    }

    let recordToSave = this._clone(record);
    this._normalizeRecord(recordToSave);

    recordToSave.guid = this._generateGUID();
    recordToSave.version = this.version;

    // Metadata
    let now = Date.now();
    recordToSave.timeCreated = now;
    recordToSave.timeLastModified = now;
    recordToSave.timeLastUsed = 0;
    recordToSave.timesUsed = 0;

    return this._saveRecord(recordToSave);
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
    }

    if (sourceSync) {
      let sync = this._getSyncMetaData(recordToSave, true);
      sync.changeCounter = 0;
    }

    this._computeFields(recordToSave);

    this._store.data[this._collectionName].push(recordToSave);

    this._store.saveSoon();

    Services.obs.notifyObservers({wrappedJSObject: {sourceSync}}, "formautofill-storage-changed", "add");
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
      let oldValue = recordFound[field];
      let newValue = recordToUpdate[field];

      if (newValue != null) {
        recordFound[field] = newValue;
      } else {
        delete recordFound[field];
      }

      this._maybeStoreLastSyncedField(recordFound, field, oldValue);
    }

    recordFound.timeLastModified = Date.now();
    let syncMetadata = this._getSyncMetaData(recordFound);
    if (syncMetadata) {
      syncMetadata.changeCounter += 1;
    }

    this._stripComputedFields(recordFound);
    this._computeFields(recordFound);

    this._store.saveSoon();
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "update");
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
    Services.obs.notifyObservers(null, "formautofill-storage-changed", "notifyUsed");
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
      let existing = this._store.data[this._collectionName][index];
      if (existing.deleted) {
        return; // already a tombstone - don't touch it.
      }
      let existingSync = this._getSyncMetaData(existing);
      if (existingSync) {
        // existing sync metadata means it has been synced. This means we must
        // leave a tombstone behind.
        this._store.data[this._collectionName][index] = {
          guid,
          timeLastModified: Date.now(),
          deleted: true,
          _sync: existingSync,
        };
        existingSync.changeCounter++;
      } else {
        // If there's no sync meta-data, this record has never been synced, so
        // we can delete it.
        this._store.data[this._collectionName].splice(index, 1);
      }
    }

    this._store.saveSoon();
    Services.obs.notifyObservers({wrappedJSObject: {sourceSync}}, "formautofill-storage-changed", "remove");
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
    let clonedRecords = records.map(r => this._clone(r));
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
   * @param   {Object} localRecord
   *          The changed local record, currently in storage.
   * @param   {Object} remoteRecord
   *          The remote record.
   * @returns {Object|null}
   *          The merged record, or `null` if there are conflicts and the
   *          records can't be merged.
   */
  _mergeSyncedRecords(localRecord, remoteRecord) {
    let sync = this._getSyncMetaData(localRecord, true);

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
        isLocalSame = lastSyncedValue == sha512(localRecord[field]);
        isRemoteSame = lastSyncedValue == sha512(remoteRecord[field]);
      } else {
        // Otherwise, if the field hasn't changed since the last sync, we know
        // it's the same locally.
        isLocalSame = true;
        isRemoteSame = localRecord[field] == remoteRecord[field];
      }

      let value;
      if (isLocalSame && isRemoteSame) {
        // Local and remote are the same; doesn't matter which one we pick.
        value = localRecord[field];
      } else if (isLocalSame && !isRemoteSame) {
        value = remoteRecord[field];
      } else if (!isLocalSame && isRemoteSame) {
        // We don't need to bump the change counter when taking the local
        // change, because the counter must already be > 0 if we're attempting
        // a three-way merge.
        value = localRecord[field];
      } else if (localRecord[field] == remoteRecord[field]) {
        // Shared parent doesn't match either local or remote, but the values
        // are identical, so there's no conflict.
        value = localRecord[field];
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
    let localRecord = this._store.data[this._collectionName][index];
    let newRecord = this._clone(remoteRecord);

    this._stripComputedFields(newRecord);

    this._store.data[this._collectionName][index] = newRecord;

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
   * @param   {Object} localRecord
   *          The local record.
   * @returns {string}
   *          A clone of the local record with a new GUID.
   */
  _forkLocalRecord(localRecord) {
    let forkedLocalRecord = this._clone(localRecord);

    this._stripComputedFields(forkedLocalRecord);

    forkedLocalRecord.guid = this._generateGUID();
    this._store.data[this._collectionName].push(forkedLocalRecord);

    // Give the record fresh Sync metadata and bump its change counter as a
    // side effect. This also excludes the forked record from de-duping on the
    // next sync, if the current sync is interrupted before the record can be
    // uploaded.
    this._getSyncMetaData(forkedLocalRecord, true);

    this._computeFields(forkedLocalRecord);

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

    let localRecord = this._store.data[this._collectionName][localIndex];
    let sync = this._getSyncMetaData(localRecord, true);

    let forkedGUID = null;

    if (sync.changeCounter === 0) {
      // Local not modified. Replace local with remote.
      this._replaceRecordAt(localIndex, remoteRecord, {
        keepSyncMetadata: false,
      });
    } else {
      let mergedRecord = this._mergeSyncedRecords(localRecord, remoteRecord);
      if (mergedRecord) {
        // Local and remote modified, but we were able to merge. Replace the
        // local record with the merged record.
        this._replaceRecordAt(localIndex, mergedRecord, {
          keepSyncMetadata: true,
        });
      } else {
        // Merge conflict. Fork the local record, then replace the original
        // with the merged record.
        let forkedLocalRecord = this._forkLocalRecord(localRecord);
        forkedGUID = forkedLocalRecord.guid;
        this._replaceRecordAt(localIndex, remoteRecord, {
          keepSyncMetadata: false,
        });
      }
    }

    this._store.saveSoon();
    Services.obs.notifyObservers({wrappedJSObject: {
      sourceSync: true,
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
      this._store.data[this._collectionName].push(tombstone);
      return;
    }

    let existing = this._store.data[this._collectionName][index];
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
    this._store.data[this._collectionName][index] = {
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

    let profiles = this._store.data[this._collectionName];
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
    for (let record of this._store.data[this._collectionName]) {
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
    let profile = this._store.data[this._collectionName][index];
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
   * @param   {Object} record
   *          The remote record.
   * @returns {string|null}
   *          The GUID of the matching local record, or `null` if no records
   *          match.
   */
  findDuplicateGUID(record) {
    if (!record.guid) {
      throw new Error("Record missing GUID");
    }
    this._ensureMatchingVersion(record);
    if (record.deleted) {
      // Tombstones don't carry enough info to de-dupe, and we should have
      // handled them separately when applying the record.
      throw new Error("Tombstones can't have duplicates");
    }
    let records = this._store.data[this._collectionName];
    for (let profile of records) {
      if (profile.deleted) {
        continue;
      }
      if (profile.guid == record.guid) {
        throw new Error(`Record ${record.guid} already exists`);
      }
      if (this._getSyncMetaData(profile)) {
        // This record has already been uploaded, so it can't be a dupe of
        // another incoming item.
        continue;
      }
      let keys = new Set(Object.keys(record));
      for (let key of Object.keys(profile)) {
        keys.add(key);
      }
      // Ignore internal and computed fields when matching records. Internal
      // fields are synced, but almost certainly have different values than the
      // local record, and we'll update them in `reconcile`. Computed fields
      // aren't synced at all.
      for (let field of INTERNAL_FIELDS) {
        keys.delete(field);
      }
      for (let field of this.VALID_COMPUTED_FIELDS) {
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
        same = key in profile == key in record && profile[key] == record[key];
        if (!same) {
          break;
        }
      }
      if (same) {
        return profile.guid;
      }
    }
    return null;
  }

  /**
   * Internal helper functions.
   */

  _clone(record) {
    let result = {};
    for (let key in record) {
      if (!key.startsWith("_")) {
        result[key] = record[key];
      }
    }
    return result;
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

  // A test-only helper.
  _nukeAllRecords() {
    this._store.data[this._collectionName] = [];
    // test-only, so there's no good reason to request a save!
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
