/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "ReadingList",
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Log.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "SQLiteStore",
  "resource:///modules/readinglist/SQLiteStore.jsm");

// We use Sync's "Utils" module for the device name, which is unfortunate,
// but let's give it a better name here.
XPCOMUtils.defineLazyGetter(this, "SyncUtils", function() {
  const {Utils} = Cu.import("resource://services-sync/util.js", {});
  return Utils;
});

{ // Prevent the parent log setup from leaking into the global scope.
  let parentLog = Log.repository.getLogger("readinglist");
  parentLog.level = Preferences.get("browser.readinglist.logLevel", Log.Level.Warn);
  Preferences.observe("browser.readinglist.logLevel", value => {
    parentLog.level = value;
  });
  let formatter = new Log.BasicFormatter();
  parentLog.addAppender(new Log.ConsoleAppender(formatter));
  parentLog.addAppender(new Log.DumpAppender(formatter));
}
let log = Log.repository.getLogger("readinglist.api");


// Each ReadingListItem has a _record property, an object containing the raw
// data from the server and local store.  These are the names of the properties
// in that object.
//
// Not important, but FYI: The order that these are listed in follows the order
// that the server doc lists the fields in the article data model, more or less:
// http://readinglist.readthedocs.org/en/latest/model.html
const ITEM_RECORD_PROPERTIES = `
  guid
  serverLastModified
  url
  preview
  title
  resolvedURL
  resolvedTitle
  excerpt
  archived
  deleted
  favorite
  isArticle
  wordCount
  unread
  addedBy
  addedOn
  storedOn
  markedReadBy
  markedReadOn
  readPosition
  syncStatus
`.trim().split(/\s+/);

// Article objects that are passed to ReadingList.addItem may contain
// some properties that are known but are not currently stored in the
// ReadingList records. This is the list of properties that are knowingly
// disregarded before the item is normalized.
const ITEM_DISREGARDED_PROPERTIES = `
  byline
  dir
  content
  length
`.trim().split(/\s+/);

// Each local item has a syncStatus indicating the state of the item in relation
// to the sync server.  See also Sync.jsm.
const SYNC_STATUS_SYNCED = 0;
const SYNC_STATUS_NEW = 1;
const SYNC_STATUS_CHANGED_STATUS = 2;
const SYNC_STATUS_CHANGED_MATERIAL = 3;
const SYNC_STATUS_DELETED = 4;

// These options are passed as the "control" options to store methods and filter
// out all records in the store with syncStatus SYNC_STATUS_DELETED.
const STORE_OPTIONS_IGNORE_DELETED = {
  syncStatus: [
    SYNC_STATUS_SYNCED,
    SYNC_STATUS_NEW,
    SYNC_STATUS_CHANGED_STATUS,
    SYNC_STATUS_CHANGED_MATERIAL,
  ],
};

// Changes to the following item properties are considered "status," or
// "status-only," changes, in relation to the sync server.  Changes to other
// properties are considered "material" changes.  See also Sync.jsm.
const SYNC_STATUS_PROPERTIES_STATUS = `
  favorite
  markedReadBy
  markedReadOn
  readPosition
  unread
`.trim().split(/\s+/);


/**
 * A reading list contains ReadingListItems.
 *
 * A list maintains only one copy of an item per URL.  So if for example you use
 * an iterator to get two references to items with the same URL, your references
 * actually refer to the same JS object.
 *
 * Options Objects
 * ---------------
 *
 * Some methods on ReadingList take an "optsList", a variable number of
 * arguments, each of which is an "options object".  Options objects let you
 * control the items that the method acts on.
 *
 * Each options object is a simple object with properties whose names are drawn
 * from ITEM_RECORD_PROPERTIES.  For an item to match an options object, the
 * properties of the item must match all the properties in the object.  For
 * example, an object { guid: "123" } matches any item whose GUID is 123.  An
 * object { guid: "123", title: "foo" } matches any item whose GUID is 123 *and*
 * whose title is foo.
 *
 * You can pass multiple options objects as separate arguments.  For an item to
 * match multiple objects, its properties must match all the properties in at
 * least one of the objects.  For example, a list of objects { guid: "123" } and
 * { title: "foo" } matches any item whose GUID is 123 *or* whose title is
 * foo.
 *
 * The properties in an options object can be arrays, not only scalars.  When a
 * property is an array, then for an item to match, its corresponding property
 * must have a value that matches any value in the array.  For example, an
 * options object { guid: ["123", "456"] } matches any item whose GUID is either
 * 123 *or* 456.
 *
 * In addition to properties with names from ITEM_RECORD_PROPERTIES, options
 * objects can also have the following special properties:
 *
 *   * sort: The name of a property to sort on.
 *   * descending: A boolean, true to sort descending, false to sort ascending.
 *     If `sort` is given but `descending` isn't, the sort is ascending (since
 *     `descending` is falsey).
 *   * limit: Limits the number of matching items to this number.
 *   * offset: Starts matching items at this index in the results.
 *
 * Since you can pass multiple options objects in a list, you can include these
 * special properties in any number of the objects in the list, but it doesn't
 * really make sense to do so.  The last property in the list is the one that's
 * used.
 *
 * @param store Backing storage for the list.  See SQLiteStore.jsm for what this
 *        object's interface should look like.
 */
function ReadingListImpl(store) {
  this._store = store;
  this._itemsByNormalizedURL = new Map();
  this._iterators = new Set();
  this._listeners = new Set();
}

ReadingListImpl.prototype = {

  ItemRecordProperties: ITEM_RECORD_PROPERTIES,

  SyncStatus: {
    SYNCED: SYNC_STATUS_SYNCED,
    NEW: SYNC_STATUS_NEW,
    CHANGED_STATUS: SYNC_STATUS_CHANGED_STATUS,
    CHANGED_MATERIAL: SYNC_STATUS_CHANGED_MATERIAL,
    DELETED: SYNC_STATUS_DELETED,
  },

  SyncStatusProperties: {
    STATUS: SYNC_STATUS_PROPERTIES_STATUS,
  },

  /**
   * Yields the number of items in the list.
   *
   * @param optsList A variable number of options objects that control the
   *        items that are matched.  See Options Objects.
   * @return Promise<number> The number of matching items in the list.  Rejected
   *         with an Error on error.
   */
  count: Task.async(function* (...optsList) {
    return (yield this._store.count(optsList, STORE_OPTIONS_IGNORE_DELETED));
  }),

  /**
   * Checks whether a given URL is in the ReadingList already.
   *
   * @param {String/nsIURI} url - URL to check.
   * @returns {Promise} Promise that is fulfilled with a boolean indicating
   *                    whether the URL is in the list or not.
   */
  hasItemForURL: Task.async(function* (url) {
    url = normalizeURI(url);

    // This is used on every tab switch and page load of the current tab, so we
    // want it to be quick and avoid a DB query whenever possible.

    // First check if any cached items have a direct match.
    if (this._itemsByNormalizedURL.has(url)) {
      return true;
    }

    // Then check if any cached items may have a different resolved URL
    // that matches.
    for (let itemWeakRef of this._itemsByNormalizedURL.values()) {
      let item = itemWeakRef.get();
      if (item && item.resolvedURL == url) {
        return true;
      }
    }

    // Finally, fall back to the DB.
    let count = yield this.count({url: url}, {resolvedURL: url});
    return (count > 0);
  }),

  /**
   * Enumerates the items in the list that match the given options.
   *
   * @param callback Called for each item in the enumeration.  It's passed a
   *        single object, a ReadingListItem.  It may return a promise; if so,
   *        the callback will not be called for the next item until the promise
   *        is resolved.
   * @param optsList A variable number of options objects that control the
   *        items that are matched.  See Options Objects.
   * @return Promise<null> Resolved when the enumeration completes *and* the
   *         last promise returned by the callback is resolved.  Rejected with
   *         an Error on error.
   */
  forEachItem: Task.async(function* (callback, ...optsList) {
    let thisCallback = record => callback(this._itemFromRecord(record));
    yield this._forEachRecord(thisCallback, optsList, STORE_OPTIONS_IGNORE_DELETED);
  }),

  /**
   * Enumerates the GUIDs for previously synced items that are marked as being
   * locally deleted.
   */
  forEachSyncedDeletedGUID: Task.async(function* (callback, ...optsList) {
    let thisCallback = record => callback(record.guid);
    yield this._forEachRecord(thisCallback, optsList, {
      syncStatus: SYNC_STATUS_DELETED,
    });
  }),

  /**
   * See forEachItem.
   *
   * @param storeOptions An options object passed to the store as the "control"
   *        options.
   */
  _forEachRecord: Task.async(function* (callback, optsList, storeOptions) {
    let promiseChain = Promise.resolve();
    yield this._store.forEachItem(record => {
      promiseChain = promiseChain.then(() => {
        return new Promise((resolve, reject) => {
          let promise = callback(record);
          if (promise instanceof Promise) {
            return promise.then(resolve, reject);
          }
          resolve();
          return undefined;
        });
      });
    }, optsList, storeOptions);
    yield promiseChain;
  }),

  /**
   * Returns a new ReadingListItemIterator that can be used to enumerate items
   * in the list.
   *
   * @param optsList A variable number of options objects that control the
   *        items that are matched.  See Options Objects.
   * @return A new ReadingListItemIterator.
   */
  iterator(...optsList) {
    let iter = new ReadingListItemIterator(this, ...optsList);
    this._iterators.add(Cu.getWeakReference(iter));
    return iter;
  },

  /**
   * Adds an item to the list that isn't already present.
   *
   * The given object represents a new item, and the properties of the object
   * are those in ITEM_RECORD_PROPERTIES.  It may have as few or as many
   * properties that you want to set, but it must have a `url` property.
   *
   * It's an error to call this with an object whose `url` or `guid` properties
   * are the same as those of items that are already present in the list.  The
   * returned promise is rejected in that case.
   *
   * @param record A simple object representing an item.
   * @return Promise<ReadingListItem> Resolved with the new item when the list
   *         is updated.  Rejected with an Error on error.
   */
  addItem: Task.async(function* (record) {
    record = normalizeRecord(record);
    if (!record.url) {
      throw new Error("The item must have a url");
    }
    if (!("addedOn" in record)) {
      record.addedOn = Date.now();
    }
    if (!("addedBy" in record)) {
      try {
        record.addedBy = Services.prefs.getCharPref("services.sync.client.name");
      } catch (ex) {
        record.addedBy = SyncUtils.getDefaultDeviceName();
      }
    }
    if (!("syncStatus" in record)) {
      record.syncStatus = SYNC_STATUS_NEW;
    }

    log.debug("addingItem with guid: ${guid}, url: ${url}", record);
    yield this._store.addItem(record);
    log.trace("added item with guid: ${guid}, url: ${url}", record);
    this._invalidateIterators();
    let item = this._itemFromRecord(record);
    this._callListeners("onItemAdded", item);
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
    mm.broadcastAsyncMessage("Reader:Added", item);
    return item;
  }),

  /**
   * Updates the properties of an item that belongs to the list.
   *
   * The passed-in item may have as few or as many properties that you want to
   * set; only the properties that are present are updated.  The item must have
   * a `url`, however.
   *
   * It's an error to call this for an item that doesn't belong to the list.
   * The returned promise is rejected in that case.
   *
   * @param item The ReadingListItem to update.
   * @return Promise<null> Resolved when the list is updated.  Rejected with an
   *         Error on error.
   */
  updateItem: Task.async(function* (item) {
    if (!item._record.url) {
      throw new Error("The item must have a url");
    }
    this._ensureItemBelongsToList(item);
    log.debug("updatingItem with guid: ${guid}, url: ${url}", item._record);
    yield this._store.updateItem(item._record);
    log.trace("finished update of item guid: ${guid}, url: ${url}", item._record);
    this._invalidateIterators();
    this._callListeners("onItemUpdated", item);
  }),

  /**
   * Deletes an item from the list.  The item must have a `url`.
   *
   * It's an error to call this for an item that doesn't belong to the list.
   * The returned promise is rejected in that case.
   *
   * @param item The ReadingListItem to delete.
   * @return Promise<null> Resolved when the list is updated.  Rejected with an
   *         Error on error.
   */
  deleteItem: Task.async(function* (item) {
    this._ensureItemBelongsToList(item);

    // If the item is new and therefore hasn't been synced yet, delete it from
    // the store.  Otherwise mark it as deleted but don't actually delete it so
    // that its status can be synced.
    if (item._record.syncStatus == SYNC_STATUS_NEW) {
      log.debug("deleteItem guid: ${guid}, url: ${url} - item is local so really deleting it", item._record);
      yield this._store.deleteItemByURL(item.url);
    }
    else {
      // To prevent data leakage, only keep the record fields needed to sync
      // the deleted status: guid and syncStatus.
      let newRecord = {};
      for (let prop of ITEM_RECORD_PROPERTIES) {
        newRecord[prop] = null;
      }
      newRecord.guid = item._record.guid;
      newRecord.syncStatus = SYNC_STATUS_DELETED;
      log.debug("deleteItem guid: ${guid}, url: ${url} - item has been synced so updating to deleted state", item._record);
      yield this._store.updateItemByGUID(newRecord);
    }

    log.trace("finished db operation deleting item with guid: ${guid}, url: ${url}", item._record);
    item.list = null;
    // failing to remove the item from the map points at something bad!
    if (!this._itemsByNormalizedURL.delete(item.url)) {
      log.error("Failed to remove item from the map", item);
    }
    this._invalidateIterators();
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
    mm.broadcastAsyncMessage("Reader:Removed", item);
    this._callListeners("onItemDeleted", item);
  }),

  /**
   * Finds the first item that matches the given options.
   *
   * @param optsList See Options Objects.
   * @return The first matching item, or null if there are no matching items.
   */
  item: Task.async(function* (...optsList) {
    return (yield this.iterator(...optsList).items(1))[0] || null;
  }),

  /**
   * Find any item that matches a given URL - either the item's URL, or its
   * resolved URL.
   *
   * @param {String/nsIURI} uri - URI to match against. This will be normalized.
   * @return The first matching item, or null if there are no matching items.
   */
  itemForURL: Task.async(function* (uri) {
    let url = normalizeURI(uri);
    return (yield this.item({ url: url }, { resolvedURL: url }));
  }),

  /**
   * Add to the ReadingList the page that is loaded in a given browser.
   *
   * @param {<xul:browser>} browser - Browser element for the document,
   * used to get metadata about the article.
   * @param {nsIURI/string} url - url to add to the reading list.
   * @return {Promise} Promise that is fullfilled with the added item.
   */
  addItemFromBrowser: Task.async(function* (browser, url) {
    let metadata = yield getMetadataFromBrowser(browser);
    let record = {
      url: url,
      title: metadata.title,
      resolvedURL: metadata.url,
      excerpt: metadata.description,
    };

    if (metadata.previews.length > 0) {
      record.preview = metadata.previews[0];
    }

    return (yield this.addItem(record));
  }),

  /**
   * Adds a listener that will be notified when the list changes.  Listeners
   * are objects with the following optional methods:
   *
   *   onItemAdded(item)
   *   onItemUpdated(item)
   *   onItemDeleted(item)
   *
   * @param listener A listener object.
   */
  addListener(listener) {
    this._listeners.add(listener);
  },

  /**
   * Removes a listener from the list.
   *
   * @param listener A listener object.
   */
  removeListener(listener) {
    this._listeners.delete(listener);
  },

  /**
   * Call this when you're done with the list.  Don't use it afterward.
   */
  destroy: Task.async(function* () {
    yield this._store.destroy();
    for (let itemWeakRef of this._itemsByNormalizedURL.values()) {
      let item = itemWeakRef.get();
      if (item) {
        item.list = null;
      }
    }
    this._itemsByNormalizedURL.clear();
  }),

  // The list's backing store.
  _store: null,

  // A Map mapping *normalized* URL strings to nsIWeakReferences that refer to
  // ReadingListItems.
  _itemsByNormalizedURL: null,

  // A Set containing nsIWeakReferences that refer to valid iterators produced
  // by the list.
  _iterators: null,

  // A Set containing listener objects.
  _listeners: null,

  /**
   * Returns the ReadingListItem represented by the given record object.  If
   * the item doesn't exist yet, it's created first.
   *
   * @param record A simple object with *normalized* item record properties.
   * @return The ReadingListItem.
   */
  _itemFromRecord(record) {
    if (!record.url) {
      throw new Error("record must have a URL");
    }
    let itemWeakRef = this._itemsByNormalizedURL.get(record.url);
    let item = itemWeakRef ? itemWeakRef.get() : null;
    if (item) {
      item._record = record;
    }
    else {
      item = new ReadingListItem(record);
      item.list = this;
      this._itemsByNormalizedURL.set(record.url, Cu.getWeakReference(item));
    }
    return item;
  },

  /**
   * Marks all the list's iterators as invalid, meaning it's not safe to use
   * them anymore.
   */
  _invalidateIterators() {
    for (let iterWeakRef of this._iterators) {
      let iter = iterWeakRef.get();
      if (iter) {
        iter.invalidate();
      }
    }
    this._iterators.clear();
  },

  /**
   * Calls a method on all listeners.
   *
   * @param methodName The name of the method to call.
   * @param item This item will be passed to the listeners.
   */
  _callListeners(methodName, item) {
    for (let listener of this._listeners) {
      if (methodName in listener) {
        try {
          listener[methodName](item);
        }
        catch (err) {
          Cu.reportError(err);
        }
      }
    }
  },

  _ensureItemBelongsToList(item) {
    if (!item || !item._ensureBelongsToList) {
      throw new Error("The item is not a ReadingListItem");
    }
    item._ensureBelongsToList();
  },
};


let _unserializable = () => {}; // See comments in the ReadingListItem ctor.

/**
 * An item in a reading list.
 *
 * Each item belongs to a list, and it's an error to use an item with a
 * ReadingList that the item doesn't belong to.
 *
 * @param record A simple object with the properties of the item, as few or many
 *        as you want.  This will be normalized.
 */
function ReadingListItem(record={}) {
  this._record = record;

  // |this._unserializable| works around a problem when sending one of these
  // items via a message manager. If |this.list| is set, the item can't be
  // transferred directly, so .toJSON is implicitly called and the object
  // returned via that is sent. However, once the item is deleted and |this.list|
  // is null, the item *can* be directly serialized - so the message handler
  // sees the "raw" object - ie, it sees "_record" etc.
  // We work around this problem by *always* having an unserializable property
  // on the object - this way the implicit .toJSON call is always made, even
  // when |this.list| is null.
  this._unserializable = _unserializable;
}

ReadingListItem.prototype = {

  // Be careful when caching properties.  If you cache a property that depends
  // on a mutable _record property, then you need to recache your property after
  // _record is set.

  /**
   * Item's unique ID.
   * @type string
   */
  get id() {
    if (!this._id) {
      this._id = hash(this.url);
    }
    return this._id;
  },

  /**
   * The item's server-side GUID. This is set by the remote server and therefore is not
   * guaranteed to be set for local items.
   * @type string
   */
  get guid() {
    return this._record.guid || undefined;
  },

  /**
   * The item's URL.
   * @type string
   */
  get url() {
    return this._record.url || undefined;
  },

  /**
   * The item's URL as an nsIURI.
   * @type nsIURI
   */
  get uri() {
    if (!this._uri) {
      this._uri = this._record.url ?
                  Services.io.newURI(this._record.url, "", null) :
                  undefined;
    }
    return this._uri;
  },

  /**
   * The item's resolved URL.
   * @type string
   */
  get resolvedURL() {
    return this._record.resolvedURL || undefined;
  },
  set resolvedURL(val) {
    this._updateRecord({ resolvedURL: val });
  },

  /**
   * The item's resolved URL as an nsIURI.  The setter takes an nsIURI or a
   * string spec.
   * @type nsIURI
   */
  get resolvedURI() {
    return this._record.resolvedURL ?
           Services.io.newURI(this._record.resolvedURL, "", null) :
           undefined;
  },
  set resolvedURI(val) {
    this._updateRecord({ resolvedURL: val });
  },

  /**
   * The item's title.
   * @type string
   */
  get title() {
    return this._record.title || undefined;
  },
  set title(val) {
    this._updateRecord({ title: val });
  },

  /**
   * The item's resolved title.
   * @type string
   */
  get resolvedTitle() {
    return this._record.resolvedTitle || undefined;
  },
  set resolvedTitle(val) {
    this._updateRecord({ resolvedTitle: val });
  },

  /**
   * The item's excerpt.
   * @type string
   */
  get excerpt() {
    return this._record.excerpt || undefined;
  },
  set excerpt(val) {
    this._updateRecord({ excerpt: val });
  },

  /**
   * The item's archived status.
   * @type boolean
   */
  get archived() {
    return !!this._record.archived;
  },
  set archived(val) {
    this._updateRecord({ archived: !!val });
  },

  /**
   * Whether the item is a favorite.
   * @type boolean
   */
  get favorite() {
    return !!this._record.favorite;
  },
  set favorite(val) {
    this._updateRecord({ favorite: !!val });
  },

  /**
   * Whether the item is an article.
   * @type boolean
   */
  get isArticle() {
    return !!this._record.isArticle;
  },
  set isArticle(val) {
    this._updateRecord({ isArticle: !!val });
  },

  /**
   * The item's word count.
   * @type integer
   */
  get wordCount() {
    return this._record.wordCount || undefined;
  },
  set wordCount(val) {
    this._updateRecord({ wordCount: val });
  },

  /**
   * Whether the item is unread.
   * @type boolean
   */
  get unread() {
    return !!this._record.unread;
  },
  set unread(val) {
    this._updateRecord({ unread: !!val });
  },

  /**
   * The date the item was added.
   * @type Date
   */
  get addedOn() {
    return this._record.addedOn ?
           new Date(this._record.addedOn) :
           undefined;
  },
  set addedOn(val) {
    this._updateRecord({ addedOn: val.valueOf() });
  },

  /**
   * The date the item was stored.
   * @type Date
   */
  get storedOn() {
    return this._record.storedOn ?
           new Date(this._record.storedOn) :
           undefined;
  },
  set storedOn(val) {
    this._updateRecord({ storedOn: val.valueOf() });
  },

  /**
   * The GUID of the device that marked the item read.
   * @type string
   */
  get markedReadBy() {
    return this._record.markedReadBy || undefined;
  },
  set markedReadBy(val) {
    this._updateRecord({ markedReadBy: val });
  },

  /**
   * The date the item marked read.
   * @type Date
   */
  get markedReadOn() {
    return this._record.markedReadOn ?
           new Date(this._record.markedReadOn) :
           undefined;
  },
  set markedReadOn(val) {
    this._updateRecord({ markedReadOn: val.valueOf() });
  },

  /**
   * The item's read position.
   * @param integer
   */
  get readPosition() {
    return this._record.readPosition || undefined;
  },
  set readPosition(val) {
    this._updateRecord({ readPosition: val });
  },

  /**
   * The URL to a preview image.
   * @type string
   */
   get preview() {
     return this._record.preview || undefined;
   },

  /**
   * Deletes the item from its list.
   *
   * @return Promise<null> Resolved when the list has been updated.
   */
  delete: Task.async(function* () {
    this._ensureBelongsToList();
    yield this.list.deleteItem(this);
    this.delete = () => Promise.reject("The item has already been deleted");
  }),

  toJSON() {
    return this._record;
  },

  /**
   * Do not use this at all unless you know what you're doing.  Use the public
   * getters and setters, above, instead.
   *
   * A simple object that contains the item's normalized data in the same format
   * that the local store and server use.  Records passed in by the consumer are
   * not normalized, but everywhere else, records are always normalized unless
   * otherwise stated.  The setter normalizes the passed-in value, so it will
   * throw an error if the value is not a valid record.
   *
   * This object should reflect the item's representation in the local store, so
   * when calling the setter, be careful that it doesn't drift away from the
   * store's record.  If you set it, you should also call updateItem() around
   * the same time.
   */
  get _record() {
    return this.__record;
  },
  set _record(val) {
    this.__record = normalizeRecord(val);
  },

  /**
   * Updates the item's record.  This calls the _record setter, so it will throw
   * an error if the partial record is not valid.
   *
   * @param partialRecord An object containing any of the record properties.
   */
  _updateRecord(partialRecord) {
    let record = this._record;

    // The syncStatus flag can change from SYNCED to either CHANGED_STATUS or
    // CHANGED_MATERIAL, or from CHANGED_STATUS to CHANGED_MATERIAL.
    if (record.syncStatus == SYNC_STATUS_SYNCED ||
        record.syncStatus == SYNC_STATUS_CHANGED_STATUS) {
      let allStatusChanges = Object.keys(partialRecord).every(prop => {
        return SYNC_STATUS_PROPERTIES_STATUS.indexOf(prop) >= 0;
      });
      record.syncStatus = allStatusChanges ? SYNC_STATUS_CHANGED_STATUS :
                          SYNC_STATUS_CHANGED_MATERIAL;
    }

    for (let prop in partialRecord) {
      record[prop] = partialRecord[prop];
    }
    this._record = record;
  },

  _ensureBelongsToList() {
    if (!this.list) {
      throw new Error("The item must belong to a reading list");
    }
  },
};

/**
 * An object that enumerates over items in a list.
 *
 * You can enumerate items a chunk at a time by passing counts to forEach() and
 * items().  An iterator remembers where it left off, so for example calling
 * forEach() with a count of 10 will enumerate the first 10 items, and then
 * calling it again with 10 will enumerate the next 10 items.
 *
 * It's possible for an iterator's list to be modified between calls to
 * forEach() and items().  If that happens, the iterator is no longer safe to
 * use, so it's invalidated.  You can check whether an iterator is invalid by
 * getting its `invalid` property.  Attempting to use an invalid iterator will
 * throw an error.
 *
 * @param list The ReadingList to enumerate.
 * @param optsList A variable number of options objects that control the items
 *        that are matched.  See Options Objects.
 */
function ReadingListItemIterator(list, ...optsList) {
  this.list = list;
  this.index = 0;
  this.optsList = optsList;
}

ReadingListItemIterator.prototype = {

  /**
   * True if it's not safe to use the iterator.  Attempting to use an invalid
   * iterator will throw an error.
   */
  invalid: false,

  /**
   * Enumerates the items in the iterator starting at its current index.  The
   * iterator is advanced by the number of items enumerated.
   *
   * @param callback Called for each item in the enumeration.  It's passed a
   *        single object, a ReadingListItem.  It may return a promise; if so,
   *        the callback will not be called for the next item until the promise
   *        is resolved.
   * @param count The maximum number of items to enumerate.  Pass -1 to
   *        enumerate them all.
   * @return Promise<null> Resolved when the enumeration completes *and* the
   *         last promise returned by the callback is resolved.
   */
  forEach: Task.async(function* (callback, count=-1) {
    this._ensureValid();
    let optsList = clone(this.optsList);
    optsList.push({
      offset: this.index,
      limit: count,
    });
    yield this.list.forEachItem(item => {
      this.index++;
      return callback(item);
    }, ...optsList);
  }),

  /**
   * Gets an array of items in the iterator starting at its current index.  The
   * iterator is advanced by the number of items fetched.
   *
   * @param count The maximum number of items to get.
   * @return Promise<array> The fetched items.
   */
  items: Task.async(function* (count) {
    this._ensureValid();
    let optsList = clone(this.optsList);
    optsList.push({
      offset: this.index,
      limit: count,
    });
    let items = [];
    yield this.list.forEachItem(item => items.push(item), ...optsList);
    this.index += items.length;
    return items;
  }),

  /**
   * Invalidates the iterator.  You probably don't want to call this unless
   * you're a ReadingList.
   */
  invalidate() {
    this.invalid = true;
  },

  _ensureValid() {
    if (this.invalid) {
      throw new Error("The iterator has been invalidated");
    }
  },
};


/**
 * Normalizes the properties of a record object, which represents a
 * ReadingListItem.  Throws an error if the record contains properties that
 * aren't in ITEM_RECORD_PROPERTIES.
 *
 * @param record A non-normalized record object.
 * @return The new normalized record.
 */
function normalizeRecord(nonNormalizedRecord) {
  let record = {};
  for (let prop in nonNormalizedRecord) {
    if (ITEM_DISREGARDED_PROPERTIES.indexOf(prop) >= 0) {
      continue;
    }
    if (ITEM_RECORD_PROPERTIES.indexOf(prop) < 0) {
      throw new Error("Unrecognized item property: " + prop);
    }
    switch (prop) {
    case "url":
    case "resolvedURL":
      if (nonNormalizedRecord[prop]) {
        record[prop] = normalizeURI(nonNormalizedRecord[prop]);
      }
      else {
        record[prop] = nonNormalizedRecord[prop];
      }
      break;
    default:
      record[prop] = nonNormalizedRecord[prop];
      break;
    }
  }
  return record;
}

/**
 * Normalize a URI, stripping away extraneous parts we don't want to store
 * or compare against.
 *
 * @param {nsIURI/String} uri - URI to normalize.
 * @returns {String} String spec of a cloned and normalized version of the
 *          input URI.
 */
function normalizeURI(uri) {
  if (typeof uri == "string") {
    try {
      uri = Services.io.newURI(uri, "", null);
    } catch (ex) {
      return uri;
    }
  }
  uri = uri.cloneIgnoringRef();
  try {
    uri.userPass = "";
  } catch (ex) {} // Not all nsURI impls (eg, nsSimpleURI) support .userPass
  return uri.spec;
};

function hash(str) {
  let hasher = Cc["@mozilla.org/security/hash;1"].
               createInstance(Ci.nsICryptoHash);
  hasher.init(Ci.nsICryptoHash.MD5);
  let stream = Cc["@mozilla.org/io/string-input-stream;1"].
               createInstance(Ci.nsIStringInputStream);
  stream.data = str;
  hasher.updateFromStream(stream, -1);
  let binaryStr = hasher.finish(false);
  let hexStr =
    [("0" + binaryStr.charCodeAt(i).toString(16)).slice(-2) for (i in binaryStr)].
    join("");
  return hexStr;
}

function clone(obj) {
  return Cu.cloneInto(obj, {}, { cloneFunctions: false });
}

/**
 * Get page metadata from the content document in a given <xul:browser>.
 * @see PageMetadata.jsm
 *
 * @param {<xul:browser>} browser - Browser element for the document.
 * @returns {Promise} Promise that is fulfilled with an object describing the metadata.
 */
function getMetadataFromBrowser(browser) {
  let mm = browser.messageManager;
  return new Promise(resolve => {
    function handleResult(msg) {
      mm.removeMessageListener("PageMetadata:PageDataResult", handleResult);
      resolve(msg.json);
    }
    mm.addMessageListener("PageMetadata:PageDataResult", handleResult);
    mm.sendAsyncMessage("PageMetadata:GetPageData");
  });
}

Object.defineProperty(this, "ReadingList", {
  get() {
    if (!this._singleton) {
      let store = new SQLiteStore("reading-list.sqlite");
      this._singleton = new ReadingListImpl(store);
    }
    return this._singleton;
  },
});
