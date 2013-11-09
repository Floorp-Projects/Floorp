/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict'

/* static functions */

function debug(s) {
  // dump('DEBUG DataStoreService: ' + s + '\n');
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/DataStore.jsm');
Cu.import("resource://gre/modules/DataStoreDB.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "permissionManager",
                                   "@mozilla.org/permissionmanager;1",
                                   "nsIPermissionManager");

XPCOMUtils.defineLazyServiceGetter(this, "secMan",
                                   "@mozilla.org/scriptsecuritymanager;1",
                                   "nsIScriptSecurityManager");

/* DataStoreService */

const DATASTORESERVICE_CID = Components.ID('{d193d0e2-c677-4a7b-bb0a-19155b470f2e}');
const REVISION_VOID = "void";

function DataStoreService() {
  debug('DataStoreService Constructor');

  this.inParent = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                    .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

  if (this.inParent) {
    let obs = Services.obs;
    if (!obs) {
      debug("DataStore Error: observer-service is null!");
      return;
    }

    obs.addObserver(this, 'webapps-clear-data', false);
  }

  let self = this;
  cpmm.addMessageListener("datastore-first-revision-created",
                          function(aMsg) { self.receiveMessage(aMsg); });
}

DataStoreService.prototype = {
  inParent: false,

  // Hash of DataStores
  stores: {},
  accessStores: {},
  pendingRequests: {},

  installDataStore: function(aAppId, aName, aOrigin, aOwner, aReadOnly) {
    debug('installDataStore - appId: ' + aAppId + ', aName: ' +
          aName + ', aOrigin: ' + aOrigin + ', aOwner:' + aOwner +
          ', aReadOnly: ' + aReadOnly);

    this.checkIfInParent();

    if (aName in this.stores && aAppId in this.stores[aName]) {
      debug('This should not happen');
      return;
    }

    if (!(aName in this.stores)) {
      this.stores[aName] = {};
    }

    // A DataStore is enabled when it has a first valid revision.
    this.stores[aName][aAppId] = { origin: aOrigin, owner: aOwner,
                                   readOnly: aReadOnly, enabled: false };

    this.addPermissions(aAppId, aName, aOrigin, aOwner, aReadOnly);

    this.createFirstRevisionId(aAppId, aName, aOwner);
  },

  installAccessDataStore: function(aAppId, aName, aOrigin, aOwner, aReadOnly) {
    debug('installAccessDataStore - appId: ' + aAppId + ', aName: ' +
          aName + ', aOrigin: ' + aOrigin + ', aOwner:' + aOwner +
          ', aReadOnly: ' + aReadOnly);

    this.checkIfInParent();

    if (aName in this.accessStores && aAppId in this.accessStores[aName]) {
      debug('This should not happen');
      return;
    }

    if (!(aName in this.accessStores)) {
      this.accessStores[aName] = {};
    }

    this.accessStores[aName][aAppId] = { origin: aOrigin, owner: aOwner,
                                         readOnly: aReadOnly };
    this.addAccessPermissions(aAppId, aName, aOrigin, aOwner, aReadOnly);
  },

  checkIfInParent: function() {
    if (!this.inParent) {
      throw "DataStore can execute this operation just in the parent process";
    }
  },

  createFirstRevisionId: function(aAppId, aName, aOwner) {
    debug("createFirstRevisionId database: " + aName);

    let self = this;
    let db = new DataStoreDB();
    db.init(aOwner, aName);
    db.revisionTxn(
      'readwrite',
      function(aTxn, aRevisionStore) {
        debug("createFirstRevisionId - transaction success");

        let request = aRevisionStore.openCursor(null, 'prev');
        request.onsuccess = function(aEvent) {
          let cursor = aEvent.target.result;
          if (!cursor) {
            // If the revision doesn't exist, let's create the first one.
            db.addRevision(aRevisionStore, 0, REVISION_VOID, function() {
              debug("First revision created.");
              if (aName in self.stores && aAppId in self.stores[aName]) {
                self.stores[aName][aAppId].enabled = true;
                ppmm.broadcastAsyncMessage('datastore-first-revision-created',
                                           { name: aName, owner: aOwner });
              }
            });
          }
        };
      }
    );
  },

  addPermissions: function(aAppId, aName, aOrigin, aOwner, aReadOnly) {
    // When a new DataStore is installed, the permissions must be set for the
    // owner app.
    let permission = "indexedDB-chrome-" + aName + '|' + aOwner;
    this.resetPermissions(aAppId, aOrigin, aOwner, permission, aReadOnly);

    // For any app that wants to have access to this DataStore we add the
    // permissions.
    if (aName in this.accessStores) {
      for (let appId in this.accessStores[aName]) {
        // ReadOnly is decided by the owner first.
        let readOnly = aReadOnly || this.accessStores[aName][appId].readOnly;
        this.resetPermissions(appId, this.accessStores[aName][appId].origin,
                              this.accessStores[aName][appId].owner,
                              permission, readOnly);
      }
    }
  },

  addAccessPermissions: function(aAppId, aName, aOrigin, aOwner, aReadOnly) {
    // When an app wants to have access to a DataStore, the permissions must be
    // set.
    if (!(aName in this.stores)) {
      return;
    }

    for (let appId in this.stores[aName]) {
      let permission = "indexedDB-chrome-" + aName + '|' + this.stores[aName][appId].owner;
      // The ReadOnly is decied by the owenr first.
      let readOnly = this.stores[aName][appId].readOnly || aReadOnly;
      this.resetPermissions(aAppId, aOrigin, aOwner, permission, readOnly);
    }
  },

  resetPermissions: function(aAppId, aOrigin, aOwner, aPermission, aReadOnly) {
    debug("ResetPermissions - appId: " + aAppId + " - origin: " + aOrigin +
          " - owner: " + aOwner + " - permissions: " + aPermission +
          " - readOnly: " + aReadOnly);

    let uri = Services.io.newURI(aOrigin, null, null);
    let principal = secMan.getAppCodebasePrincipal(uri, aAppId, false);

    let result = permissionManager.testExactPermissionFromPrincipal(principal,
                                                                    aPermission + '-write');

    if (aReadOnly && result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      debug("Write permission removed");
      permissionManager.removeFromPrincipal(principal, aPermission + '-write');
    } else if (!aReadOnly && result != Ci.nsIPermissionManager.ALLOW_ACTION) {
      debug("Write permission added");
      permissionManager.addFromPrincipal(principal, aPermission + '-write',
                                         Ci.nsIPermissionManager.ALLOW_ACTION);
    }

    result = permissionManager.testExactPermissionFromPrincipal(principal,
                                                                aPermission + '-read');
    if (result != Ci.nsIPermissionManager.ALLOW_ACTION) {
      debug("Read permission added");
      permissionManager.addFromPrincipal(principal, aPermission + '-read',
                                         Ci.nsIPermissionManager.ALLOW_ACTION);
    }

    result = permissionManager.testExactPermissionFromPrincipal(principal, aPermission);
    if (result != Ci.nsIPermissionManager.ALLOW_ACTION) {
      debug("Generic permission added");
      permissionManager.addFromPrincipal(principal, aPermission,
                                         Ci.nsIPermissionManager.ALLOW_ACTION);
    }
  },

  getDataStores: function(aWindow, aName) {
    debug('getDataStores - aName: ' + aName);

    let self = this;
    return new aWindow.Promise(function(resolve, reject) {
      // If this request comes from the main process, we have access to the
      // window, so we can skip the ipc communication.
      if (self.inParent) {
        let stores = self.getDataStoresInfo(aName, aWindow.document.nodePrincipal.appId);
        self.getDataStoreCreate(aWindow, resolve, stores);
      } else {
        // This method can be called in the child so we need to send a request
        // to the parent and create DataStore object here.
        new DataStoreServiceChild(aWindow, aName, function(aStores) {
          debug("DataStoreServiceChild callback!");
          self.getDataStoreCreate(aWindow, resolve, aStores);
        });
      }
    });
  },

  getDataStoresInfo: function(aName, aAppId) {
    debug('GetDataStoresInfo');

    let results = [];

    if (aName in this.stores) {
      if (aAppId in this.stores[aName]) {
        results.push({ name: aName,
                       owner: this.stores[aName][aAppId].owner,
                       readOnly: false,
                       enabled: this.stores[aName][aAppId].enabled });
      }

      for (var i in this.stores[aName]) {
        if (i == aAppId) {
          continue;
        }

        let access = this.getDataStoreAccess(aName, aAppId);
        if (!access) {
          continue;
        }

        let readOnly = this.stores[aName][i].readOnly || access.readOnly;
        results.push({ name: aName,
                       owner: this.stores[aName][i].owner,
                       readOnly: readOnly,
                       enabled: this.stores[aName][i].enabled });
      }
    }

    return results;
  },

  getDataStoreCreate: function(aWindow, aResolve, aStores) {
    debug("GetDataStoreCreate");

    let results = [];

    if (!aStores.length) {
      aResolve(results);
      return;
    }

    let pendingDataStores = [];

    for (let i = 0; i < aStores.length; ++i) {
      if (!aStores[i].enabled) {
        pendingDataStores.push(aStores[i].owner);
      }
    }

    if (!pendingDataStores.length) {
      this.getDataStoreResolve(aWindow, aResolve, aStores);
      return;
    }

    if (!(aStores[0].name in this.pendingRequests)) {
      this.pendingRequests[aStores[0].name] = [];
    }

    this.pendingRequests[aStores[0].name].push({ window: aWindow,
                                                 resolve: aResolve,
                                                 stores: aStores,
                                                 pendingDataStores: pendingDataStores });
  },

  getDataStoreResolve: function(aWindow, aResolve, aStores) {
    debug("GetDataStoreResolve");

    let callbackPending = aStores.length;
    let results = [];

    if (!callbackPending) {
      aResolve(results);
      return;
    }

    for (let i = 0; i < aStores.length; ++i) {
      let obj = new DataStore(aWindow, aStores[i].name,
                              aStores[i].owner, aStores[i].readOnly);
      let exposedObj = aWindow.DataStore._create(aWindow, obj);
      obj.exposedObject = exposedObj;

      results.push(exposedObj);

      obj.retrieveRevisionId(
        function() {
          --callbackPending;
          if (!callbackPending) {
            aResolve(results);
          }
        }
      );
    }
  },

  getDataStoreAccess: function(aName, aAppId) {
    if (!(aName in this.accessStores) ||
        !(aAppId in this.accessStores[aName])) {
      return null;
    }

    return this.accessStores[aName][aAppId];
  },

  observe: function observe(aSubject, aTopic, aData) {
    debug('getDataStores - aTopic: ' + aTopic);
    if (aTopic != 'webapps-clear-data') {
      return;
    }

    let params =
      aSubject.QueryInterface(Ci.mozIApplicationClearPrivateDataParams);

    // DataStore is explosed to apps, not browser content.
    if (params.browserOnly) {
      return;
    }

    for (let key in this.stores) {
      if (params.appId in this.stores[key]) {
        this.deleteDatabase(key, this.stores[key][params.appId].owner);
        delete this.stores[key][params.appId];
      }

      if (!this.stores[key].length) {
        delete this.stores[key];
      }
    }
  },

  deleteDatabase: function(aName, aOwner) {
    debug("delete database: " + aName);

    let db = new DataStoreDB();
    db.init(aOwner, aName);
    db.delete();
  },

  receiveMessage: function(aMsg) {
    debug("receiveMessage");
    let data = aMsg.json;

    if (!(data.name in this.pendingRequests)) {
      return;
    }

    for (let i = 0; i < this.pendingRequests[data.name].length;) {
      let pos = this.pendingRequests[data.name][i].pendingDataStores.indexOf(data.owner);
      if (pos != -1) {
        this.pendingRequests[data.name][i].pendingDataStores.splice(pos, 1);
        if (!this.pendingRequests[data.name][i].pendingDataStores.length) {
          this.getDataStoreResolve(this.pendingRequests[data.name][i].window,
                                   this.pendingRequests[data.name][i].resolve,
                                   this.pendingRequests[data.name][i].stores);
          this.pendingRequests[data.name].splice(i, 1);
          continue;
        }
      }

      ++i;
    }

    if (!this.pendingRequests[data.name].length) {
      delete this.pendingRequests[data.name];
    }
  },

  classID : DATASTORESERVICE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataStoreService,
                                         Ci.nsIObserver]),
  classInfo: XPCOMUtils.generateCI({
    classID: DATASTORESERVICE_CID,
    contractID: '@mozilla.org/datastore-service;1',
    interfaces: [Ci.nsIDataStoreService, Ci.nsIObserver],
    flags: Ci.nsIClassInfo.SINGLETON
  })
};

/* DataStoreServiceChild */

function DataStoreServiceChild(aWindow, aName, aCallback) {
  debug("DataStoreServiceChild created");
  this.init(aWindow, aName, aCallback);
}

DataStoreServiceChild.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  init: function(aWindow, aName, aCallback) {
    debug("DataStoreServiceChild init");
    this._callback = aCallback;

    this.initDOMRequestHelper(aWindow, [ "DataStore:Get:Return" ]);

    // This is a security issue and it will be fixed by Bug 916091
    cpmm.sendAsyncMessage("DataStore:Get",
                          { name: aName, appId: aWindow.document.nodePrincipal.appId });
  },

  receiveMessage: function(aMessage) {
    debug("DataStoreServiceChild receiveMessage");
    if (aMessage.name != 'DataStore:Get:Return') {
      return;
    }

    this._callback(aMessage.data.stores);
  }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DataStoreService]);
