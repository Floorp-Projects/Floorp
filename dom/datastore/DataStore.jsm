/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict'

var EXPORTED_SYMBOLS = ["DataStore", "DataStoreAccess"];

function debug(s) {
  // dump('DEBUG DataStore: ' + s + '\n');
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const REVISION_ADDED = "added";
const REVISION_UPDATED = "updated";
const REVISION_REMOVED = "removed";
const REVISION_VOID = "void";

Cu.import("resource://gre/modules/DataStoreDB.jsm");
Cu.import("resource://gre/modules/ObjectWrapper.jsm");
Cu.import('resource://gre/modules/Services.jsm');

/* Helper function */
function createDOMError(aWindow, aEvent) {
  return new aWindow.DOMError(aEvent.target.error.name);
}

/* DataStore object */

function DataStore(aAppId, aName, aOwner, aReadOnly, aGlobalScope) {
  this.appId = aAppId;
  this.name = aName;
  this.owner = aOwner;
  this.readOnly = aReadOnly;

  this.db = new DataStoreDB();
  this.db.init(aOwner, aName, aGlobalScope);
}

DataStore.prototype = {
  appId: null,
  name: null,
  owner: null,
  readOnly: null,
  revisionId: null,

  newDBPromise: function(aWindow, aTxnType, aFunction) {
    let db = this.db;
    return new aWindow.Promise(function(aResolve, aReject) {
      debug("DBPromise started");
      db.txn(
        aTxnType,
        function(aTxn, aStore, aRevisionStore) {
          debug("DBPromise success");
          aFunction(aResolve, aReject, aTxn, aStore, aRevisionStore);
        },
        function(aEvent) {
          debug("DBPromise error");
          aReject(createDOMError(aWindow, aEvent));
        }
      );
    });
  },

  getInternal: function(aWindow, aResolve, aStore, aId) {
    debug("GetInternal " + aId);

    let request = aStore.get(aId);
    request.onsuccess = function(aEvent) {
      debug("GetInternal success. Record: " + aEvent.target.result);
      aResolve(ObjectWrapper.wrap(aEvent.target.result, aWindow));
    };
  },

  updateInternal: function(aResolve, aStore, aRevisionStore, aId, aObj) {
    debug("UpdateInternal " + aId);

    let self = this;
    let request = aStore.put(aObj, aId);
    request.onsuccess = function(aEvent) {
      debug("UpdateInternal success");

      self.addRevision(aRevisionStore, aId, REVISION_UPDATED,
        function() {
          debug("UpdateInternal - revisionId increased");
          // No wrap here because the result is always a int.
          aResolve(aEvent.target.result);
        }
      );
    };
  },

  addInternal: function(aResolve, aStore, aRevisionStore, aObj) {
    debug("AddInternal");

    let self = this;
    let request = aStore.put(aObj);
    request.onsuccess = function(aEvent) {
      debug("Request successful. Id: " + aEvent.target.result);
      self.addRevision(aRevisionStore, aEvent.target.result, REVISION_ADDED,
        function() {
          debug("AddInternal - revisionId increased");
          // No wrap here because the result is always a int.
          aResolve(aEvent.target.result);
        }
      );
    };
  },

  removeInternal: function(aResolve, aStore, aRevisionStore, aId) {
    debug("RemoveInternal");

    let self = this;
    let request = aStore.get(aId);
    request.onsuccess = function(aEvent) {
      debug("RemoveInternal success. Record: " + aEvent.target.result);
      if (aEvent.target.result === undefined) {
        aResolve(false);
        return;
      }

      let deleteRequest = aStore.delete(aId);
      deleteRequest.onsuccess = function() {
        debug("RemoveInternal success");
        self.addRevision(aRevisionStore, aId, REVISION_REMOVED,
          function() {
            aResolve(true);
          }
        );
      };
    };
  },

  clearInternal: function(aResolve, aStore, aRevisionStore) {
    debug("ClearInternal");

    let self = this;
    let request = aStore.clear();
    request.onsuccess = function() {
      debug("ClearInternal success");
      self.db.clearRevisions(aRevisionStore,
        function() {
          debug("Revisions cleared");

          self.addRevision(aRevisionStore, 0, REVISION_VOID,
            function() {
              debug("ClearInternal - revisionId increased");
              aResolve();
            }
          );
        }
      );
    };
  },

  addRevision: function(aRevisionStore, aId, aType, aSuccessCb) {
    let self = this;
    this.db.addRevision(aRevisionStore, aId, aType,
      function(aRevisionId) {
        self.revisionId = aRevisionId;
        aSuccessCb();
      }
    );
  },

  retrieveRevisionId: function(aSuccessCb) {
    if (this.revisionId != null) {
      aSuccessCb();
      return;
    }

    let self = this;
    this.db.revisionTxn(
      'readwrite',
      function(aTxn, aRevisionStore) {
        debug("RetrieveRevisionId transaction success");

        let request = aRevisionStore.openCursor(null, 'prev');
        request.onsuccess = function(aEvent) {
          let cursor = aEvent.target.result;
          if (!cursor) {
            // If the revision doesn't exist, let's create the first one.
            self.addRevision(aRevisionStore, 0, REVISION_VOID, aSuccessCb);
            return;
          }

          self.revisionId = cursor.value.revisionId;
          aSuccessCb();
        };
      }
    );
  },

  throwInvalidArg: function(aWindow) {
    return aWindow.Promise.reject(
      new aWindow.DOMError("SyntaxError", "Non-numeric or invalid id"));
  },

  throwReadOnly: function(aWindow) {
    return aWindow.Promise.reject(
      new aWindow.DOMError("ReadOnlyError", "DataStore in readonly mode"));
  },

  exposeObject: function(aWindow, aReadOnly) {
    let self = this;
    let object = {

      // Public interface :

      get name() {
        return self.name;
      },

      get owner() {
        return self.owner;
      },

      get readOnly() {
        return aReadOnly;
      },

      get: function DS_get(aId) {
        aId = parseInt(aId);
        if (isNaN(aId) || aId <= 0) {
          return self.throwInvalidArg(aWindow);
        }

        // Promise<Object>
        return self.newDBPromise(aWindow, "readonly",
          function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
            self.getInternal(aWindow, aResolve, aStore, aId);
          }
        );
      },

      update: function DS_update(aId, aObj) {
        aId = parseInt(aId);
        if (isNaN(aId) || aId <= 0) {
          return self.throwInvalidArg(aWindow);
        }

        if (aReadOnly) {
          return self.throwReadOnly(aWindow);
        }

        // Promise<void>
        return self.newDBPromise(aWindow, "readwrite",
          function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
            self.updateInternal(aResolve, aStore, aRevisionStore, aId, aObj);
          }
        );
      },

      add: function DS_add(aObj) {
        if (aReadOnly) {
          return self.throwReadOnly(aWindow);
        }

        // Promise<int>
        return self.newDBPromise(aWindow, "readwrite",
          function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
            self.addInternal(aResolve, aStore, aRevisionStore, aObj);
          }
        );
      },

      remove: function DS_remove(aId) {
        aId = parseInt(aId);
        if (isNaN(aId) || aId <= 0) {
          return self.throwInvalidArg(aWindow);
        }

        if (aReadOnly) {
          return self.throwReadOnly(aWindow);
        }

        // Promise<void>
        return self.newDBPromise(aWindow, "readwrite",
          function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
            self.removeInternal(aResolve, aStore, aRevisionStore, aId);
          }
        );
      },

      clear: function DS_clear() {
        if (aReadOnly) {
          return self.throwReadOnly(aWindow);
        }

        // Promise<void>
        return self.newDBPromise(aWindow, "readwrite",
          function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
            self.clearInternal(aResolve, aStore, aRevisionStore);
          }
        );
      },

      get revisionId() {
        return self.revisionId;
      },

      getChanges: function(aRevisionId) {
        debug("GetChanges: " + aRevisionId);

        if (aRevisionId === null || aRevisionId === undefined) {
          return aWindow.Promise.reject(
            new aWindow.DOMError("SyntaxError", "Invalid revisionId"));
        }

        // Promise<DataStoreChanges>
        return new aWindow.Promise(function(aResolve, aReject) {
          debug("GetChanges promise started");
          self.db.revisionTxn(
            'readonly',
            function(aTxn, aStore) {
              debug("GetChanges transaction success");

              let request = self.db.getInternalRevisionId(
                aRevisionId,
                aStore,
                function(aInternalRevisionId) {
                  if (aInternalRevisionId == undefined) {
                    aResolve(undefined);
                    return;
                  }

                  // This object is the return value of this promise.
                  // Initially we use maps, and then we convert them in array.
                  let changes = {
                    revisionId: '',
                    addedIds: {},
                    updatedIds: {},
                    removedIds: {}
                  };

                  let request = aStore.mozGetAll(aWindow.IDBKeyRange.lowerBound(aInternalRevisionId, true));
                  request.onsuccess = function(aEvent) {
                    for (let i = 0; i < aEvent.target.result.length; ++i) {
                      let data = aEvent.target.result[i];

                      switch (data.operation) {
                        case REVISION_ADDED:
                          changes.addedIds[data.objectId] = true;
                          break;

                        case REVISION_UPDATED:
                          // We don't consider an update if this object has been added
                          // or if it has been already modified by a previous
                          // operation.
                          if (!(data.objectId in changes.addedIds) &&
                              !(data.objectId in changes.updatedIds)) {
                            changes.updatedIds[data.objectId] = true;
                          }
                          break;

                        case REVISION_REMOVED:
                          let id = data.objectId;

                          // If the object has been added in this range of revisions
                          // we can ignore it and remove it from the list.
                          if (id in changes.addedIds) {
                            delete changes.addedIds[id];
                          } else {
                            changes.removedIds[id] = true;
                          }

                          if (id in changes.updatedIds) {
                            delete changes.updatedIds[id];
                          }
                          break;
                      }
                    }

                    // The last revisionId.
                    if (aEvent.target.result.length) {
                      changes.revisionId = aEvent.target.result[aEvent.target.result.length - 1].revisionId;
                    }

                    // From maps to arrays.
                    changes.addedIds = Object.keys(changes.addedIds).map(function(aKey) { return parseInt(aKey, 10); });
                    changes.updatedIds = Object.keys(changes.updatedIds).map(function(aKey) { return parseInt(aKey, 10); });
                    changes.removedIds = Object.keys(changes.removedIds).map(function(aKey) { return parseInt(aKey, 10); });

                    let wrappedObject = ObjectWrapper.wrap(changes, aWindow);
                    aResolve(wrappedObject);
                  };
                }
              );
            },
            function(aEvent) {
              debug("GetChanges transaction failed");
              aReject(createDOMError(aWindow, aEvent));
            }
          );
        });
      },

      /* TODO:
         attribute EventHandler onchange;
         getAll(), getLength()
       */

      __exposedProps__: {
        name: 'r',
        owner: 'r',
        readOnly: 'r',
        get: 'r',
        update: 'r',
        add: 'r',
        remove: 'r',
        clear: 'r',
        revisionId: 'r',
        getChanges: 'r'
      }
    };

    return object;
  },

  delete: function() {
    this.db.delete();
  }
};

/* DataStoreAccess */

function DataStoreAccess(aAppId, aName, aOrigin, aReadOnly) {
  this.appId = aAppId;
  this.name = aName;
  this.origin = aOrigin;
  this.readOnly = aReadOnly;
}

DataStoreAccess.prototype = {};

