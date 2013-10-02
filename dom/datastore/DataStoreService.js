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

    let store = new DataStore(aAppId, aName, aOwner, aReadOnly);

    if (!(aName in this.stores)) {
      this.stores[aName] = {};
    }

    this.stores[aName][aAppId] = store;
  },

  installAccessDataStore: function(aAppId, aName, aOwner, aReadOnly) {
    debug('installDataStore - appId: ' + aAppId + ', aName: ' +
          aName + ', aOwner:' + aOwner + ', aReadOnly: ' +
          aReadOnly);

    if (aName in this.accessStores && aAppId in this.accessStores[aName]) {
      debug('This should not happen');
      return;
    }

    let accessStore = new DataStoreAccess(aAppId, aName, aOwner, aReadOnly);

    if (!(aName in this.accessStores)) {
      this.accessStores[aName] = {};
    }

    this.accessStores[aName][aAppId] = accessStore;
  },

  getDataStores: function(aWindow, aName) {
    debug('getDataStores - aName: ' + aName);
    let appId = aWindow.document.nodePrincipal.appId;

    let self = this;
    return new aWindow.Promise(function(resolve, reject) {
      let matchingStores = [];

      if (aName in self.stores) {
        if (appId in self.stores[aName]) {
          matchingStores.push({ store: self.stores[aName][appId],
                                readonly: false });
        }

        for (var i in self.stores[aName]) {
          if (i == appId) {
            continue;
          }

          let access = self.getDataStoreAccess(self.stores[aName][i], appId);
          if (!access) {
            continue;
          }

          let readOnly = self.stores[aName][i].readOnly || access.readOnly;
          matchingStores.push({ store: self.stores[aName][i],
                                readonly: readOnly });
        }
      }

      let callbackPending = matchingStores.length;
      let results = [];

      if (!callbackPending) {
        resolve(results);
        return;
      }

      for (let i = 0; i < matchingStores.length; ++i) {
        let obj = matchingStores[i].store.exposeObject(aWindow,
                                    matchingStores[i].readonly);
        results.push(obj);

        matchingStores[i].store.retrieveRevisionId(
          function() {
            --callbackPending;
            if (!callbackPending) {
              resolve(results);
            }
          },
          function() {
            reject();
          }
        );
      }
    });
  },

  getDataStoreAccess: function(aStore, aAppId) {
    if (!(aStore.name in this.accessStores) ||
        !(aAppId in this.accessStores[aStore.name])) {
      return null;
    }

    return this.accessStores[aStore.name][aAppId];
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
        this.stores[key][params.appId].delete();
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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DataStoreService]);
