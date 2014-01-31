/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict'

this.EXPORTED_SYMBOLS = ["DataStore"];

function debug(s) {
  //dump('DEBUG DataStore: ' + s + '\n');
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const REVISION_ADDED = "added";
const REVISION_UPDATED = "updated";
const REVISION_REMOVED = "removed";
const REVISION_VOID = "void";

// This value has to be tuned a bit. Currently it's just a guess
// and yet we don't know if it's too low or too high.
const MAX_REQUESTS = 25;

Cu.import("resource://gre/modules/DataStoreCursor.jsm");
Cu.import("resource://gre/modules/DataStoreDB.jsm");
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.importGlobalProperties(["indexedDB"]);

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

/* Helper functions */
function createDOMError(aWindow, aEvent) {
  return new aWindow.DOMError(aEvent);
}

function throwInvalidArg(aWindow) {
  return aWindow.Promise.reject(
    new aWindow.DOMError("SyntaxError", "Non-numeric or invalid id"));
}

function throwReadOnly(aWindow) {
  return aWindow.Promise.reject(
    new aWindow.DOMError("ReadOnlyError", "DataStore in readonly mode"));
}

function validateId(aId) {
  // If string, it cannot be empty.
  if (typeof(aId) == 'string') {
    return aId.length;
  }

  aId = parseInt(aId);
  return (!isNaN(aId) && aId > 0);
}

/* DataStore object */
this.DataStore = function(aWindow, aName, aOwner, aReadOnly) {
  debug("DataStore created");
  this.init(aWindow, aName, aOwner, aReadOnly);
}

this.DataStore.prototype = {
  classDescription: "DataStore XPCOM Component",
  classID: Components.ID("{db5c9602-030f-4bff-a3de-881a8de370f2}"),
  contractID: "@mozilla.org/dom/datastore;1",
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISupports]),

  callbacks: [],

  _window: null,
  _name: null,
  _owner: null,
  _readOnly: null,
  _revisionId: null,
  _exposedObject: null,
  _cursor: null,

  init: function(aWindow, aName, aOwner, aReadOnly) {
    debug("DataStore init");

    this._window = aWindow;
    this._name = aName;
    this._owner = aOwner;
    this._readOnly = aReadOnly;

    this._db = new DataStoreDB();
    this._db.init(aOwner, aName);

    let self = this;
    Services.obs.addObserver(function(aSubject, aTopic, aData) {
      let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (wId == self._innerWindowID) {
        cpmm.removeMessageListener("DataStore:Changed:Return:OK", self);
        self._db.close();
      }
    }, "inner-window-destroyed", false);

    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    this._innerWindowID = util.currentInnerWindowID;

    cpmm.addMessageListener("DataStore:Changed:Return:OK", this);
    cpmm.sendAsyncMessage("DataStore:RegisterForMessages",
                          { store: this._name, owner: this._owner });
  },

  newDBPromise: function(aTxnType, aFunction) {
    let self = this;
    return new this._window.Promise(function(aResolve, aReject) {
      debug("DBPromise started");
      self._db.txn(
        aTxnType,
        function(aTxn, aStore, aRevisionStore) {
          debug("DBPromise success");
          aFunction(aResolve, aReject, aTxn, aStore, aRevisionStore);
        },
        function(aEvent) {
          debug("DBPromise error");
          aReject(createDOMError(self._window, aEvent));
        }
      );
    });
  },

  getInternal: function(aStore, aIds, aCallback) {
    debug("GetInternal: " + aIds.toSource());

    // Creation of the results array.
    let results = new Array(aIds.length);

    // We're going to create this amount of requests.
    let pendingIds = aIds.length;
    let indexPos = 0;

    let self = this;

    function getInternalSuccess(aEvent, aPos) {
      debug("GetInternal success. Record: " + aEvent.target.result);
      results[aPos] = Cu.cloneInto(aEvent.target.result, self._window);
      if (!--pendingIds) {
        aCallback(results);
        return;
      }

      if (indexPos < aIds.length) {
        // Just MAX_REQUESTS requests at the same time.
        let count = 0;
        while (indexPos < aIds.length && ++count < MAX_REQUESTS) {
          getInternalRequest();
        }
      }
    }

    function getInternalRequest() {
      let currentPos = indexPos++;
      let request = aStore.get(aIds[currentPos]);
      request.onsuccess = function(aEvent) {
        getInternalSuccess(aEvent, currentPos);
      }
    }

    getInternalRequest();
  },

  putInternal: function(aResolve, aStore, aRevisionStore, aObj, aId) {
    debug("putInternal " + aId);

    let self = this;
    let request = aStore.put(aObj, aId);
    request.onsuccess = function(aEvent) {
      debug("putInternal success");

      self.addRevision(aRevisionStore, aId, REVISION_UPDATED,
        function() {
          debug("putInternal - revisionId increased");
          // No wrap here because the result is always a int.
          aResolve(aEvent.target.result);
        }
      );
    };
  },

  addInternal: function(aResolve, aStore, aRevisionStore, aObj, aId) {
    debug("AddInternal");

    let self = this;
    let request = aStore.add(aObj, aId);
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
      self._db.clearRevisions(aRevisionStore,
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

  getLengthInternal: function(aResolve, aStore) {
    debug("GetLengthInternal");

    let request = aStore.count();
    request.onsuccess = function(aEvent) {
      debug("GetLengthInternal success: " + aEvent.target.result);
      // No wrap here because the result is always a int.
      aResolve(aEvent.target.result);
    };
  },

  addRevision: function(aRevisionStore, aId, aType, aSuccessCb) {
    let self = this;
    this._db.addRevision(aRevisionStore, aId, aType,
      function(aRevisionId) {
        self._revisionId = aRevisionId;
        self.sendNotification(aId, aType, aRevisionId);
        aSuccessCb();
      }
    );
  },

  retrieveRevisionId: function(aSuccessCb) {
    let self = this;
    this._db.revisionTxn(
      'readonly',
      function(aTxn, aRevisionStore) {
        debug("RetrieveRevisionId transaction success");

        let request = aRevisionStore.openCursor(null, 'prev');
        request.onsuccess = function(aEvent) {
          let cursor = aEvent.target.result;
          if (cursor) {
            self._revisionId = cursor.value.revisionId;
          }

          aSuccessCb(self._revisionId);
        };
      }
    );
  },

  sendNotification: function(aId, aOperation, aRevisionId) {
    debug("SendNotification");
    if (aOperation == REVISION_VOID) {
      aOperation = "cleared";
    }

    cpmm.sendAsyncMessage("DataStore:Changed",
                          { store: this.name, owner: this.owner,
                            message: { revisionId: aRevisionId, id: aId,
                                       operation: aOperation } } );
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage");

    if (aMessage.name != "DataStore:Changed:Return:OK") {
      debug("Wrong message: " + aMessage.name);
      return;
    }

    let self = this;

    this.retrieveRevisionId(
      function() {
        // If we have an active cursor we don't emit events.
        if (self._cursor) {
          return;
        }

        let event = new self._window.DataStoreChangeEvent('change', aMessage.data);
        self.__DOM_IMPL__.dispatchEvent(event);
      }
    );
  },

  get exposedObject() {
    debug("get exposedObject");
    return this._exposedObject;
  },

  set exposedObject(aObject) {
    debug("set exposedObject");
    this._exposedObject = aObject;
  },

  syncTerminated: function(aCursor) {
    // This checks is to avoid that an invalid cursor stops a sync.
    if (this._cursor == aCursor) {
      this._cursor = null;
    }
  },

  // Public interface :

  get name() {
    return this._name;
  },

  get owner() {
    return this._owner;
  },

  get readOnly() {
    return this._readOnly;
  },

  get: function() {
    let ids = Array.prototype.slice.call(arguments);
    for (let i = 0; i < ids.length; ++i) {
      if (!validateId(ids[i])) {
        return throwInvalidArg(this._window);
      }
    }

    let self = this;

    // Promise<Object>
    return this.newDBPromise("readonly",
      function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
               self.getInternal(aStore, ids,
                                function(aResults) {
          aResolve(ids.length > 1 ? aResults : aResults[0]);
        });
      }
    );
  },

  put: function(aObj, aId) {
    if (!validateId(aId)) {
      return throwInvalidArg(this._window);
    }

    if (this._readOnly) {
      return throwReadOnly(this._window);
    }

    let self = this;

    // Promise<void>
    return this.newDBPromise("readwrite",
      function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
        self.putInternal(aResolve, aStore, aRevisionStore, aObj, aId);
      }
    );
  },

  add: function(aObj, aId) {
    if (aId) {
      if (!validateId(aId)) {
        return throwInvalidArg(this._window);
      }
    }

    if (this._readOnly) {
      return throwReadOnly(this._window);
    }

    let self = this;

    // Promise<int>
    return this.newDBPromise("readwrite",
      function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
        self.addInternal(aResolve, aStore, aRevisionStore, aObj, aId);
      }
    );
  },

  remove: function(aId) {
    if (!validateId(aId)) {
      return throwInvalidArg(this._window);
    }

    if (this._readOnly) {
      return throwReadOnly(this._window);
    }

    let self = this;

    // Promise<void>
    return this.newDBPromise("readwrite",
      function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
        self.removeInternal(aResolve, aStore, aRevisionStore, aId);
      }
    );
  },

  clear: function() {
    if (this._readOnly) {
      return throwReadOnly(this._window);
    }

    let self = this;

    // Promise<void>
    return this.newDBPromise("readwrite",
      function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
        self.clearInternal(aResolve, aStore, aRevisionStore);
      }
    );
  },

  get revisionId() {
    return this._revisionId;
  },

  getLength: function() {
    let self = this;

    // Promise<int>
    return this.newDBPromise("readonly",
      function(aResolve, aReject, aTxn, aStore, aRevisionStore) {
        self.getLengthInternal(aResolve, aStore);
      }
    );
  },

  set onchange(aCallback) {
    debug("Set OnChange");
    this.__DOM_IMPL__.setEventHandler("onchange", aCallback);
  },

  get onchange() {
    debug("Get OnChange");
    return this.__DOM_IMPL__.getEventHandler("onchange");
  },

  sync: function(aRevisionId) {
    debug("Sync");
    this._cursor = new DataStoreCursor(this._window, this, aRevisionId);
    return this._window.DataStoreCursor._create(this._window, this._cursor);
  }
};
