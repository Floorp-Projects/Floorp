/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["AddressesEngine", "CreditCardsEngine"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "profileStorage",
                                  "resource://formautofill/ProfileStorage.jsm");

// A helper to sanitize address and creditcard records suitable for logging.
function sanitizeStorageObject(ob) {
  if (!ob) {
    return null;
  }
  const whitelist = ["timeCreated", "timeLastUsed", "timeLastModified"];
  let result = {};
  for (let key of Object.keys(ob)) {
    let origVal = ob[key];
    if (whitelist.includes(key)) {
      result[key] = origVal;
    } else if (typeof origVal == "string") {
      result[key] = "X".repeat(origVal.length);
    } else {
      result[key] = typeof(origVal); // *shrug*
    }
  }
  return result;
}


function AutofillRecord(collection, id) {
  CryptoWrapper.call(this, collection, id);
}

AutofillRecord.prototype = {
  __proto__: CryptoWrapper.prototype,

  toEntry() {
    return Object.assign({
      guid: this.id,
    }, this.entry);
  },

  fromEntry(entry) {
    this.id = entry.guid;
    this.entry = entry;
    // The GUID is already stored in record.id, so we nuke it from the entry
    // itself to save a tiny bit of space. The profileStorage clones profiles,
    // so nuking in-place is OK.
    delete this.entry.guid;
  },

  cleartextToString() {
    // And a helper so logging a *Sync* record auto sanitizes.
    let record = this.cleartext;
    return JSON.stringify({entry: sanitizeStorageObject(record.entry)});
  },
};

// Profile data is stored in the "entry" object of the record.
Utils.deferGetSet(AutofillRecord, "cleartext", ["entry"]);

function FormAutofillStore(name, engine) {
  Store.call(this, name, engine);
}

FormAutofillStore.prototype = {
  __proto__: Store.prototype,

  _subStorageName: null, // overridden below.
  _storage: null,

  get storage() {
    if (!this._storage) {
      this._storage = profileStorage[this._subStorageName];
    }
    return this._storage;
  },

  async getAllIDs() {
    let result = {};
    for (let {guid} of this.storage.getAll({includeDeleted: true})) {
      result[guid] = true;
    }
    return result;
  },

  async changeItemID(oldID, newID) {
    this.storage.changeGUID(oldID, newID);
  },

  // Note: this function intentionally returns false in cases where we only have
  // a (local) tombstone - and profileStorage.get() filters them for us.
  async itemExists(id) {
    return Boolean(this.storage.get(id));
  },

  async applyIncoming(remoteRecord) {
    if (remoteRecord.deleted) {
      this._log.trace("Deleting record", remoteRecord);
      this.storage.remove(remoteRecord.id, {sourceSync: true});
      return;
    }

    if (await this.itemExists(remoteRecord.id)) {
      // We will never get a tombstone here, so we are updating a real record.
      await this._doUpdateRecord(remoteRecord);
      return;
    }

    // No matching local record. Try to dedupe a NEW local record.
    let localDupeID = this.storage.findDuplicateGUID(remoteRecord.toEntry());
    if (localDupeID) {
      this._log.trace(`Deduping local record ${localDupeID} to remote`, remoteRecord);
      // Change the local GUID to match the incoming record, then apply the
      // incoming record.
      await this.changeItemID(localDupeID, remoteRecord.id);
      await this._doUpdateRecord(remoteRecord);
      return;
    }

    // We didn't find a dupe, either, so must be a new record (or possibly
    // a non-deleted version of an item we have a tombstone for, which add()
    // handles for us.)
    this._log.trace("Add record", remoteRecord);
    let entry = remoteRecord.toEntry();
    this.storage.add(entry, {sourceSync: true});
  },

  async createRecord(id, collection) {
    this._log.trace("Create record", id);
    let record = new AutofillRecord(collection, id);
    let entry = this.storage.get(id, {
      rawData: true,
    });
    if (entry) {
      record.fromEntry(entry);
    } else {
      // We should consider getting a more authortative indication it's actually deleted.
      this._log.debug(`Failed to get autofill record with id "${id}", assuming deleted`);
      record.deleted = true;
    }
    return record;
  },

  async _doUpdateRecord(record) {
    this._log.trace("Updating record", record);

    let entry = record.toEntry();
    let {forkedGUID} = this.storage.reconcile(entry);
    if (this._log.level <= Log.Level.Debug) {
      let forkedRecord = forkedGUID ? this.storage.get(forkedGUID) : null;
      let reconciledRecord = this.storage.get(record.id);
      this._log.debug("Updated local record", {
        forked: sanitizeStorageObject(forkedRecord),
        updated: sanitizeStorageObject(reconciledRecord),
      });
    }
  },

  // NOTE: Because we re-implement the incoming/reconcilliation logic we leave
  // the |create|, |remove| and |update| methods undefined - the base
  // implementation throws, which is what we want to happen so we can identify
  // any places they are "accidentally" called.
};

function FormAutofillTracker(name, engine) {
  Tracker.call(this, name, engine);
}

FormAutofillTracker.prototype = {
  __proto__: Tracker.prototype,
  observe: function observe(subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);
    if (topic != "formautofill-storage-changed") {
      return;
    }
    if (subject && subject.wrappedJSObject && subject.wrappedJSObject.sourceSync) {
      return;
    }
    switch (data) {
      case "add":
      case "update":
      case "remove":
        this.score += SCORE_INCREMENT_XLARGE;
        break;
      default:
        this._log.debug("unrecognized autofill notification", data);
        break;
    }
  },

  // `_ignore` checks the change source for each observer notification, so we
  // don't want to let the engine ignore all changes during a sync.
  get ignoreAll() {
    return false;
  },

  // Define an empty setter so that the engine doesn't throw a `TypeError`
  // setting a read-only property.
  set ignoreAll(value) {},

  startTracking() {
    Services.obs.addObserver(this, "formautofill-storage-changed");
  },

  stopTracking() {
    Services.obs.removeObserver(this, "formautofill-storage-changed");
  },

  // We never want to persist changed IDs, as the changes are already stored
  // in ProfileStorage
  persistChangedIDs: false,

  // Ensure we aren't accidentally using the base persistence.
  get changedIDs() {
    throw new Error("changedIDs isn't meaningful for this engine");
  },

  set changedIDs(obj) {
    throw new Error("changedIDs isn't meaningful for this engine");
  },

  addChangedID(id, when) {
    throw new Error("Don't add IDs to the autofill tracker");
  },

  removeChangedID(id) {
    throw new Error("Don't remove IDs from the autofill tracker");
  },

  // This method is called at various times, so we override with a no-op
  // instead of throwing.
  clearChangedIDs() {},
};

// This uses the same conventions as BookmarkChangeset in
// services/sync/modules/engines/bookmarks.js. Specifically,
// - "synced" means the item has already been synced (or we have another reason
//   to ignore it), and should be ignored in most methods.
class AutofillChangeset extends Changeset {
  constructor() {
    super();
  }

  getModifiedTimestamp(id) {
    throw new Error("Don't use timestamps to resolve autofill merge conflicts");
  }

  has(id) {
    let change = this.changes[id];
    if (change) {
      return !change.synced;
    }
    return false;
  }

  delete(id) {
    let change = this.changes[id];
    if (change) {
      // Mark the change as synced without removing it from the set. We do this
      // so that we can update ProfileStorage in `trackRemainingChanges`.
      change.synced = true;
    }
  }
}

function FormAutofillEngine(service, name) {
  SyncEngine.call(this, name, service);
}

FormAutofillEngine.prototype = {
  __proto__: SyncEngine.prototype,

  // the priority for this engine is == addons, so will happen after bookmarks
  // prefs and tabs, but before forms, history, etc.
  syncPriority: 5,

  // We don't use SyncEngine.initialize() for this, as we initialize even if
  // the engine is disabled, and we don't want to be the loader of
  // ProfileStorage in this case.
  async _syncStartup() {
    await profileStorage.initialize();
    await SyncEngine.prototype._syncStartup.call(this);
  },

  // We handle reconciliation in the store, not the engine.
  async _reconcile() {
    return true;
  },

  emptyChangeset() {
    return new AutofillChangeset();
  },

  async _uploadOutgoing() {
    this._modified.replace(this._store.storage.pullSyncChanges());
    await SyncEngine.prototype._uploadOutgoing.call(this);
  },

  // Typically, engines populate the changeset before downloading records.
  // However, we handle conflict resolution in the store, so we can wait
  // to pull changes until we're ready to upload.
  async pullAllChanges() {
    return {};
  },

  async pullNewChanges() {
    return {};
  },

  async trackRemainingChanges() {
    this._store.storage.pushSyncChanges(this._modified.changes);
  },

  _deleteId(id) {
    this._noteDeletedId(id);
  },

  async _resetClient() {
    this._store.storage.resetSync();
  },
};

// The concrete engines

function AddressesRecord(collection, id) {
  AutofillRecord.call(this, collection, id);
}

AddressesRecord.prototype = {
  __proto__: AutofillRecord.prototype,
  _logName: "Sync.Record.Addresses",
};

function AddressesStore(name, engine) {
  FormAutofillStore.call(this, name, engine);
}

AddressesStore.prototype = {
  __proto__: FormAutofillStore.prototype,
  _subStorageName: "addresses",
};

function AddressesEngine(service) {
  FormAutofillEngine.call(this, service, "Addresses");
}

AddressesEngine.prototype = {
  __proto__: FormAutofillEngine.prototype,
  _trackerObj: FormAutofillTracker,
  _storeObj: AddressesStore,
  _recordObj: AddressesRecord,

  get prefName() {
    return "addresses";
  },
};

function CreditCardsRecord(collection, id) {
  AutofillRecord.call(this, collection, id);
}

CreditCardsRecord.prototype = {
  __proto__: AutofillRecord.prototype,
  _logName: "Sync.Record.CreditCards",
};

function CreditCardsStore(name, engine) {
  FormAutofillStore.call(this, name, engine);
}

CreditCardsStore.prototype = {
  __proto__: FormAutofillStore.prototype,
  _subStorageName: "creditCards",
};

function CreditCardsEngine(service) {
  FormAutofillEngine.call(this, service, "CreditCards");
}

CreditCardsEngine.prototype = {
  __proto__: FormAutofillEngine.prototype,
  _trackerObj: FormAutofillTracker,
  _storeObj: CreditCardsStore,
  _recordObj: CreditCardsRecord,
  get prefName() {
    return "creditcards";
  },
};
