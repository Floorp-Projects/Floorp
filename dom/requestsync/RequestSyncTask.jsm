/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

this.EXPORTED_SYMBOLS = ['RequestSyncTask'];

function debug(s) {
  //dump('DEBUG RequestSyncTask: ' + s + '\n');
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

this.RequestSyncTask = function(aManager, aWindow, aApp, aData) {
  debug('created');

  this._manager = aManager;
  this._window = aWindow;
  this._app = aApp;

  let keys = [ 'task', 'lastSync', 'oneShot', 'minInterval', 'wakeUpPage',
               'wifiOnly', 'data', 'state', 'overwrittenMinInterval' ];
  for (let i = 0; i < keys.length; ++i) {
    if (!(keys[i] in aData)) {
      dump("ERROR - RequestSyncTask must receive a fully app object: " + keys[i] + " missing.");
      throw "ERROR!";
    }

    this["_" + keys[i]] = aData[keys[i]];
  }
}

this.RequestSyncTask.prototype = {
  classDescription: 'RequestSyncTask XPCOM Component',
  classID: Components.ID('{a1e1c9c6-ce42-49d4-b8b4-fbd686d8fdd9}'),
  contractID: '@mozilla.org/dom/request-sync-manager;1',
  QueryInterface: XPCOMUtils.generateQI([]),

  get app() {
    return this._app;
  },

  get state() {
    return this._state;
  },

  get overwrittenMinInterval() {
    return this._overwrittenMinInterval;
  },

  get task() {
    return this._task;
  },

  get lastSync() {
    return this._lastSync;
  },

  get wakeUpPage() {
    return this._wakeUpPage;
  },

  get oneShot() {
    return this._oneShot;
  },

  get minInterval() {
    return this._minInterval;
  },

  get wifiOnly() {
    return this._wifiOnly;
  },

  get data() {
    return this._data;
  },

  setPolicy: function(aState, aOverwrittenMinInterval) {
    debug("setPolicy");
    let self = this;

    return new this._window.Promise(function(aResolve, aReject) {
      let p = self._manager.setPolicy(self._task, self._app.origin,
                                      self._app.manifestURL,
                                      self._app.isInBrowserElement,
                                      aState,
                                      aOverwrittenMinInterval);

      // Set the new value only when the promise is resolved.
      p.then(function() {
        self._state = aState;
        self._overwrittenMinInterval = aOverwrittenMinInterval;
        aResolve();
      }, aReject);
    });
  }
};
