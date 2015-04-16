/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Sync",
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReadingList",
  "resource:///modules/readinglist/ReadingList.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ServerClient",
  "resource:///modules/readinglist/ServerClient.jsm");

// The maximum number of sub-requests per POST /batch supported by the server.
// See http://readinglist.readthedocs.org/en/latest/api/batch.html.
const BATCH_REQUEST_LIMIT = 25;

// The Last-Modified header of server responses is stored here.
const SERVER_LAST_MODIFIED_HEADER_PREF = "readinglist.sync.serverLastModified";

// Maps local record properties to server record properties.
const SERVER_PROPERTIES_BY_LOCAL_PROPERTIES = {
  guid: "id",
  serverLastModified: "last_modified",
  url: "url",
  preview: "preview",
  title: "title",
  resolvedURL: "resolved_url",
  resolvedTitle: "resolved_title",
  excerpt: "excerpt",
  archived: "archived",
  deleted: "deleted",
  favorite: "favorite",
  isArticle: "is_article",
  wordCount: "word_count",
  unread: "unread",
  addedBy: "added_by",
  addedOn: "added_on",
  storedOn: "stored_on",
  markedReadBy: "marked_read_by",
  markedReadOn: "marked_read_on",
  readPosition: "read_position",
};

// Local record properties that can be uploaded in new items.
const NEW_RECORD_PROPERTIES = `
  url
  title
  resolvedURL
  resolvedTitle
  excerpt
  favorite
  isArticle
  wordCount
  unread
  addedBy
  addedOn
  markedReadBy
  markedReadOn
  readPosition
  preview
`.trim().split(/\s+/);

// Local record properties that can be uploaded in changed items.
const MUTABLE_RECORD_PROPERTIES = `
  title
  resolvedURL
  resolvedTitle
  excerpt
  favorite
  isArticle
  wordCount
  unread
  markedReadBy
  markedReadOn
  readPosition
  preview
`.trim().split(/\s+/);

let log = Log.repository.getLogger("readinglist.sync");


/**
 * An object that syncs reading list state with a server.  To sync, make a new
 * SyncImpl object and then call start() on it.
 *
 * @param readingList The ReadingList to sync.
 */
function SyncImpl(readingList) {
  this.list = readingList;
  this._client = new ServerClient();
}

/**
 * This implementation uses the sync algorithm described here:
 * https://github.com/mozilla-services/readinglist/wiki/Client-phases
 * The "phases" mentioned in the methods below refer to the phases in that
 * document.
 */
SyncImpl.prototype = {

  /**
   * Starts sync, if it's not already started.
   *
   * @return Promise<null> this.promise, i.e., a promise that will be resolved
   *         when sync completes, rejected on error.
   */
  start() {
    if (!this.promise) {
      this.promise = Task.spawn(function* () {
        try {
          yield this._start();
        } finally {
          delete this.promise;
        }
      }.bind(this));
    }
    return this.promise;
  },

  /**
   * A Promise<null> that will be non-null when sync is ongoing.  Resolved when
   * sync completes, rejected on error.
   */
  promise: null,

  /**
   * See the document linked above that describes the sync algorithm.
   */
  _start: Task.async(function* () {
    log.info("Starting sync");
    yield this._logDiagnostics();
    yield this._uploadStatusChanges();
    yield this._uploadNewItems();
    yield this._uploadDeletedItems();
    yield this._downloadModifiedItems();

    // TODO: "Repeat [this phase] until no conflicts occur," says the doc.
    yield this._uploadMaterialChanges();

    log.info("Sync done");
  }),

  /**
   * Phase 0 - for debugging we log some stuff about the local store before
   * we start syncing.
   * We only do this when the log level is "Trace" or lower as the info (a)
   * may be expensive to generate, (b) generate alot of output and (c) may
   * contain private information.
   */
  _logDiagnostics: Task.async(function* () {
    // Sadly our log is likely to have Log.Level.All, so loop over our
    // appenders looking for the effective level.
    let smallestLevel = log.appenders.reduce(
      (prev, appender) => Math.min(prev, appender.level),
      Log.Level.Error);

    if (smallestLevel > Log.Level.Trace) {
      return;
    }

    let localItems = [];
    yield this.list.forEachItem(localItem => localItems.push(localItem));
    log.trace("Have " + localItems.length + " local item(s)");
    for (let localItem of localItems) {
      // We need to use .record so we get access to a couple of the "internal" fields.
      let record = localItem._record;
      let redacted = {};
      for (let attr of ["guid", "url", "resolvedURL", "serverLastModified", "syncStatus"]) {
        redacted[attr] = record[attr];
      }
      log.trace(JSON.stringify(redacted));
    }
    // and the GUIDs of deleted items.
    let deletedGuids = []
    yield this.list.forEachSyncedDeletedGUID(guid => deletedGuids.push(guid));
    // This might be a huge line, but that's OK.
    log.trace("Have ${num} deleted item(s): ${deletedGuids}", {num: deletedGuids.length, deletedGuids});
  }),

  /**
   * Phase 1 part 1
   *
   * Uploads not-new items with status-only changes.  By design, status-only
   * changes will never conflict with what's on the server.
   */
  _uploadStatusChanges: Task.async(function* () {
    log.debug("Phase 1 part 1: Uploading status changes");
    yield this._uploadChanges(ReadingList.SyncStatus.CHANGED_STATUS,
                              ReadingList.SyncStatusProperties.STATUS);
  }),

  /**
   * There are two phases for uploading changed not-new items: one for items
   * with status-only changes, one for items with material changes.  The two
   * work similarly mechanically, and this method is a helper for both.
   *
   * @param syncStatus Local items matching this sync status will be uploaded.
   * @param localProperties An array of local record property names.  The
   *        uploaded item records will include only these properties.
   */
  _uploadChanges: Task.async(function* (syncStatus, localProperties) {
    // Get local items that match the given syncStatus.
    let requests = [];
    yield this.list.forEachItem(localItem => {
      requests.push({
        path: "/articles/" + localItem.guid,
        body: serverRecordFromLocalItem(localItem, localProperties),
      });
    }, { syncStatus: syncStatus });
    if (!requests.length) {
      log.debug("No local changes to upload");
      return;
    }

    // Send the request.
    let request = {
      body: {
        defaults: {
          method: "PATCH",
        },
        requests: requests,
      },
    };
    let batchResponse = yield this._postBatch(request);
    if (batchResponse.status != 200) {
      this._handleUnexpectedResponse(true, "uploading changes", batchResponse);
      return;
    }

    // Update local items based on the response.
    for (let response of batchResponse.body.responses) {
      if (response.status == 404) {
        // item deleted
        yield this._deleteItemForGUID(response.body.id);
        continue;
      }
      if (response.status == 409) {
        // "Conflict": A change violated a uniqueness constraint.  Mark the item
        // as having material changes, and reconcile and upload it in the
        // material-changes phase.
        // TODO
        continue;
      }
      if (response.status != 200) {
        this._handleUnexpectedResponse(false, "uploading a change", response);
        continue;
      }
      // Don't assume the local record and the server record aren't materially
      // different.  Reconcile the differences.
      // TODO

      let item = yield this._itemForGUID(response.body.id);
      yield this._updateItemWithServerRecord(item, response.body);
    }
  }),

  /**
   * Phase 1 part 2
   *
   * Uploads new items.
   */
  _uploadNewItems: Task.async(function* () {
    log.debug("Phase 1 part 2: Uploading new items");

    // Get new local items.
    let requests = [];
    yield this.list.forEachItem(localItem => {
      requests.push({
        body: serverRecordFromLocalItem(localItem, NEW_RECORD_PROPERTIES),
      });
    }, { syncStatus: ReadingList.SyncStatus.NEW });
    if (!requests.length) {
      log.debug("No new local items to upload");
      return;
    }

    // Send the request.
    let request = {
      body: {
        defaults: {
          method: "POST",
          path: "/articles",
        },
        requests: requests,
      },
    };
    let batchResponse = yield this._postBatch(request);
    if (batchResponse.status != 200) {
      this._handleUnexpectedResponse(true, "uploading new items", batchResponse);
      return;
    }

    // Update local items based on the response.
    for (let response of batchResponse.body.responses) {
      if (response.status == 303) {
        // "See Other": An item with the URL already exists.  Mark the item as
        // having material changes, and reconcile and upload it in the
        // material-changes phase.
        // TODO
        continue;
      }
      // Note that the server seems to return a 200 if an identical item already
      // exists, but we shouldn't be uploading identical items in this phase in
      // normal usage. But if something goes wrong locally (eg, we upload but
      // get some error even though the upload worked) we will see this.
      // So allow 200 but log a warning.
      if (response.status == 200) {
        log.debug("Attempting to upload a new item found the server already had it", response);
        // but we still process it.
      } else if (response.status != 201) {
        this._handleUnexpectedResponse(false, "uploading a new item", response);
        continue;
      }
      let item = yield this.list.itemForURL(response.body.url);
      yield this._updateItemWithServerRecord(item, response.body);
    }
  }),

  /**
   * Phase 1 part 3
   *
   * Uploads deleted synced items.
   */
  _uploadDeletedItems: Task.async(function* () {
    log.debug("Phase 1 part 3: Uploading deleted items");

    // Get deleted synced local items.
    let requests = [];
    yield this.list.forEachSyncedDeletedGUID(guid => {
      requests.push({
        path: "/articles/" + guid,
      });
    });
    if (!requests.length) {
      log.debug("No local deleted synced items to upload");
      return;
    }

    // Send the request.
    let request = {
      body: {
        defaults: {
          method: "DELETE",
        },
        requests: requests,
      },
    };
    let batchResponse = yield this._postBatch(request);
    if (batchResponse.status != 200) {
      this._handleUnexpectedResponse(true, "uploading deleted items", batchResponse);
      return;
    }

    // Delete local items based on the response.
    for (let response of batchResponse.body.responses) {
      // A 404 means the item was already deleted on the server, which is OK.
      // We still need to make sure it's deleted locally, though.
      if (response.status != 200 && response.status != 404) {
        this._handleUnexpectedResponse(false, "uploading a deleted item", response);
        continue;
      }
      yield this._deleteItemForGUID(response.body.id);
    }
  }),

  /**
   * Phase 2
   *
   * Downloads items that were modified since the last sync.
   */
  _downloadModifiedItems: Task.async(function* () {
    log.debug("Phase 2: Downloading modified items");

    // Get modified items from the server.
    let path = "/articles";
    if (this._serverLastModifiedHeader) {
      path += "?_since=" + this._serverLastModifiedHeader;
    }
    let request = {
      method: "GET",
      path: path,
    };
    let response = yield this._sendRequest(request);
    if (response.status != 200) {
      this._handleUnexpectedResponse(true, "downloading modified items", response);
      return;
    }

    // Update local items based on the response.
    for (let serverRecord of response.body.items) {
      if (serverRecord.deleted) {
        // _deleteItemForGUID is a no-op if no item exists with the GUID.
        yield this._deleteItemForGUID(serverRecord.id);
        continue;
      }
      let localItem = yield this._itemForGUID(serverRecord.id);
      if (localItem) {
        if (localItem.serverLastModified == serverRecord.last_modified) {
          // We just uploaded this item in the new-items phase.
          continue;
        }
        // The local item may have materially changed.  In that case, don't
        // overwrite the local changes with the server record.  Instead, mark
        // the item as having material changes and reconcile and upload it in
        // the material-changes phase.
        // TODO

        yield this._updateItemWithServerRecord(localItem, serverRecord);
        continue;
      }
      // A potentially new item.  addItem() will fail here when an item was
      // added to the local list between the time we uploaded new items and
      // now.
      let localRecord = localRecordFromServerRecord(serverRecord);
      try {
        yield this.list.addItem(localRecord);
      } catch (ex) {
        if (ex instanceof ReadingList.Error.Exists) {
          log.debug("Tried to add an item that already exists.");
        } else {
          log.error("Error adding an item from server record ${serverRecord} ${ex}",
                    { serverRecord, ex });
        }
      }
    }

    // Now that changes have been successfully applied, advance the server
    // last-modified timestamp so that next time we fetch items starting from
    // the current point.  Response header names are lowercase.
    if (response.headers && "last-modified" in response.headers) {
      this._serverLastModifiedHeader = response.headers["last-modified"];
    }
  }),

  /**
   * Phase 3 (material changes)
   *
   * Uploads not-new items with material changes.
   */
  _uploadMaterialChanges: Task.async(function* () {
    log.debug("Phase 3: Uploading material changes");
    yield this._uploadChanges(ReadingList.SyncStatus.CHANGED_MATERIAL,
                              MUTABLE_RECORD_PROPERTIES);
  }),

  /**
   * Gets the local ReadingListItem with the given GUID.
   *
   * @param guid The item's GUID.
   * @return The matching ReadingListItem.
   */
  _itemForGUID: Task.async(function* (guid) {
    return (yield this.list.item({ guid: guid }));
  }),

  /**
   * Updates the given local ReadingListItem with the given server record.  The
   * local item's sync status is updated to reflect the fact that the item has
   * been synced and is up to date.
   *
   * @param item A local ReadingListItem.
   * @param serverRecord A server record representing the item.
   */
  _updateItemWithServerRecord: Task.async(function* (localItem, serverRecord) {
    if (!localItem) {
      // The item may have been deleted from the local list between the time we
      // saw that it needed updating and now.
      log.debug("Tried to update a null local item from server record",
                serverRecord);
      return;
    }
    localItem._record = localRecordFromServerRecord(serverRecord);
    try {
      yield this.list.updateItem(localItem);
    } catch (ex) {
      // The item may have been deleted from the local list after we fetched it.
      if (ex instanceof ReadingList.Error.Deleted) {
        log.debug("Tried to update an item that was deleted from server record",
                  serverRecord);
      } else {
        log.error("Error updating an item from server record ${serverRecord} ${ex}",
                  { serverRecord, ex });
      }
    }
  }),

  /**
   * Truly deletes the local ReadingListItem with the given GUID.
   *
   * @param guid The item's GUID.
   */
  _deleteItemForGUID: Task.async(function* (guid) {
    let item = yield this._itemForGUID(guid);
    if (item) {
      // If item is non-null, then it hasn't been deleted locally.  Therefore
      // it's important to delete it through its list so that the list and its
      // consumers are notified properly.  Set the syncStatus to NEW so that the
      // list truly deletes the item.
      item._record.syncStatus = ReadingList.SyncStatus.NEW;
      try {
        yield this.list.deleteItem(item);
      } catch (ex) {
        log.error("Failed delete local item with id ${guid} ${ex}",
                  { guid, ex });
      }
      return;
    }
    // If item is null, then it may not actually exist locally, or it may have
    // been synced and then deleted so that it's marked as being deleted.  In
    // that case, try to delete it directly from the store.  As far as the list
    // is concerned, the item has already been deleted.
    log.debug("Item not present in list, deleting it by GUID instead");
    try {
      this.list._store.deleteItemByGUID(guid);
    } catch (ex) {
      log.error("Failed to delete local item with id ${guid} ${ex}",
                { guid, ex });
    }
  }),

  /**
   * Sends a request to the server.
   *
   * @param req The request object: { method, path, body, headers }.
   * @return Promise<response> Resolved with the server's response object:
   *         { status, body, headers }.
   */
  _sendRequest: Task.async(function* (req) {
    log.debug("Sending request", req);
    let response = yield this._client.request(req);
    log.debug("Received response", response);
    return response;
  }),

  /**
   * The server limits the number of sub-requests in POST /batch'es to
   * BATCH_REQUEST_LIMIT.  This method takes an arbitrarily big batch request
   * and breaks it apart into many individual batch requests in order to stay
   * within the limit.
   *
   * @param bigRequest The same type of request object that _sendRequest takes.
   *        Since it's a POST /batch request, its `body` should have a
   *        `requests` property whose value is an array of sub-requests.
   *        `method` and `path` are automatically filled.
   * @return Promise<response> Resolved when all requests complete with 200s, or
   *         when the first response that is not a 200 is received.  In the
   *         first case, the resolved response is a combination of all the
   *         server responses, and response.body.responses contains the sub-
   *         responses for all the sub-requests in bigRequest.  In the second
   *         case, the resolved response is the non-200 response straight from
   *         the server.
   */
  _postBatch: Task.async(function* (bigRequest) {
    log.debug("Sending batch requests");
    let allSubResponses = [];
    let remainingSubRequests = bigRequest.body.requests;
    while (remainingSubRequests.length) {
      let request = Object.assign({}, bigRequest);
      request.method = "POST";
      request.path = "/batch";
      request.body.requests =
        remainingSubRequests.splice(0, BATCH_REQUEST_LIMIT);
      let response = yield this._sendRequest(request);
      if (response.status != 200) {
        return response;
      }
      allSubResponses = allSubResponses.concat(response.body.responses);
    }
    let bigResponse = {
      status: 200,
      body: {
        responses: allSubResponses,
      },
    };
    log.debug("All batch requests successfully sent");
    return bigResponse;
  }),

  _handleUnexpectedResponse(isTopLevel, contextMsgFragment, response) {
    log.error(`Unexpected response ${contextMsgFragment}`, response);
    // We want to throw in some cases so the sync engine knows there was an
    // error and retries using the error schedule. 401 implies an auth issue
    // (possibly transient, possibly not) - but things like 404 might just
    // relate to a single item and need not throw.  Any 5XX implies a
    // (hopefully transient) server error.
    if (isTopLevel && (response.status == 401 || response.status >= 500)) {
      throw new Error("Sync aborted due to " + response.status + " server response.");
    }
  },

  // TODO: Wipe this pref when user logs out.
  get _serverLastModifiedHeader() {
    if (!("__serverLastModifiedHeader" in this)) {
      this.__serverLastModifiedHeader =
        Preferences.get(SERVER_LAST_MODIFIED_HEADER_PREF, undefined);
    }
    return this.__serverLastModifiedHeader;
  },
  set _serverLastModifiedHeader(val) {
    this.__serverLastModifiedHeader = val;
    Preferences.set(SERVER_LAST_MODIFIED_HEADER_PREF, val);
  },
};


/**
 * Translates a local ReadingListItem into a server record.
 *
 * @param localItem The local ReadingListItem.
 * @param localProperties An array of local item property names.  Only these
 *        properties will be included in the server record.
 * @return The server record.
 */
function serverRecordFromLocalItem(localItem, localProperties) {
  let serverRecord = {};
  for (let localProp of localProperties) {
    let serverProp = SERVER_PROPERTIES_BY_LOCAL_PROPERTIES[localProp];
    if (localProp in localItem._record) {
      serverRecord[serverProp] = localItem._record[localProp];
    }
  }
  return serverRecord;
}

/**
 * Translates a server record into a local record.  The returned local record's
 * syncStatus will reflect the fact that the local record is up-to-date synced.
 *
 * @param serverRecord The server record.
 * @return The local record.
 */
function localRecordFromServerRecord(serverRecord) {
  let localRecord = {
    // Mark the record as being up-to-date synced.
    syncStatus: ReadingList.SyncStatus.SYNCED,
  };
  for (let localProp in SERVER_PROPERTIES_BY_LOCAL_PROPERTIES) {
    let serverProp = SERVER_PROPERTIES_BY_LOCAL_PROPERTIES[localProp];
    if (serverProp in serverRecord) {
      localRecord[localProp] = serverRecord[serverProp];
    }
  }
  return localRecord;
}

Object.defineProperty(this, "Sync", {
  get() {
    if (!this._singleton) {
      this._singleton = new SyncImpl(ReadingList);
    }
    return this._singleton;
  },
});
