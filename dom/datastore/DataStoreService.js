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
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

/* DataStoreService */

const DATASTORESERVICE_CID = Components.ID('{d193d0e2-c677-4a7b-bb0a-19155b470f2e}');

function DataStoreService() {
  debug('DataStoreService Constructor');

  let obs = Services.obs;
  if (!obs) {
    debug("DataStore Error: observer-service is null!");
    return;
  }

  obs.addObserver(this, 'webapps-clear-data', false);

  let inParent = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                   .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  if (inParent) {
    ppmm.addMessageListener("DataStore:Get", this);
  }
}

DataStoreService.prototype = {
  // Hash of DataStores
  stores: {},
  accessStores: {},

  installDataStore: function(aAppId, aName, aOwner, aReadOnly) {
    debug('installDataStore - appId: ' + aAppId + ', aName: ' +
          aName + ', aOwner:' + aOwner + ', aReadOnly: ' +
          aReadOnly);

    if (aName in this.stores && aAppId in this.stores[aName]) {
      debug('This should not happen');
      return;
    }

    if (!(aName in this.stores)) {
      this.stores[aName] = {};
    }

    this.stores[aName][aAppId] = { owner: aOwner, readOnly: aReadOnly };
  },

  installAccessDataStore: function(aAppId, aName, aOwner, aReadOnly) {
    debug('installDataStore - appId: ' + aAppId + ', aName: ' +
          aName + ', aOwner:' + aOwner + ', aReadOnly: ' +
          aReadOnly);

    if (aName in this.accessStores && aAppId in this.accessStores[aName]) {
      debug('This should not happen');
      return;
    }

    if (!(aName in this.accessStores)) {
      this.accessStores[aName] = {};
    }

    this.accessStores[aName][aAppId] = { owner: aOwner, readOnly: aReadOnly };
  },

  getDataStores: function(aWindow, aName) {
    debug('getDataStores - aName: ' + aName);

    // This method can be called in the child so we need to send a request to
    // the parent and create DataStore object here.

    return new aWindow.Promise(function(resolve, reject) {
      new DataStoreServiceChild(aWindow, aName, resolve);
    });
  },

  receiveMessage: function(aMessage) {
    if (aMessage.name != 'DataStore:Get') {
      return;
    }

    let msg = aMessage.data;

    // This is a security issue and it will be fixed by Bug 916091
    let appId = msg.appId;

    let results = [];

    if (msg.name in this.stores) {
      if (appId in this.stores[msg.name]) {
        results.push({ store: this.stores[msg.name][appId],
                       readOnly: false });
      }

      for (var i in this.stores[msg.name]) {
        if (i == appId) {
          continue;
        }

        let access = this.getDataStoreAccess(msg.name, appId);
        if (!access) {
          continue;
        }

        let readOnly = this.stores[msg.name][i].readOnly || access.readOnly;
        results.push({ store: this.stores[msg.name][i],
                       readOnly: readOnly });
      }
    }

    msg.stores = results;
    aMessage.target.sendAsyncMessage("DataStore:Get:Return", msg);
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
        delete this.stores[key][params.appId];
      }

      if (!this.stores[key].length) {
        delete this.stores[key];
      }
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

function DataStoreServiceChild(aWindow, aName, aResolve) {
  debug("DataStoreServiceChild created");
  this.init(aWindow, aName, aResolve);
}

DataStoreServiceChild.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  init: function(aWindow, aName, aResolve) {
    this._window = aWindow;
    this._name = aName;
    this._resolve = aResolve;

    this.initDOMRequestHelper(aWindow, [ "DataStore:Get:Return" ]);

    // This is a security issue and it will be fixed by Bug 916091
    cpmm.sendAsyncMessage("DataStore:Get",
                          { name: aName, appId: aWindow.document.nodePrincipal.appId });
  },

  receiveMessage: function(aMessage) {
    if (aMessage.name != 'DataStore:Get:Return') {
      return;
    }
    let msg = aMessage.data;
    let self = this;

    let callbackPending = msg.stores.length;
    let results = [];

    if (!callbackPending) {
      this._resolve(results);
      return;
    }

    for (let i = 0; i < msg.stores.length; ++i) {
      let obj = new DataStore(this._window, this._name,
                              msg.stores[i].owner, msg.stores[i].readOnly);
      let exposedObj = this._window.DataStore._create(this._window, obj);
      results.push(exposedObj);

      obj.retrieveRevisionId(
        function() {
          --callbackPending;
          if (!callbackPending) {
            self._resolve(results);
          }
        }
      );
    }
  }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DataStoreService]);
