/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const {classes: Cc, interfaces: Ci, utils: Cu, manager: Cm} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

const cid = '{17a84227-28f4-453d-9b80-9ae75a5682e0}';
const contractId = '@mozilla.org/test-update-provider;1';

function TestUpdateProvider() {}
TestUpdateProvider.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemUpdateProvider]),

  checkForUpdate: function() {
    dump('check for update');
    this._listener.onUpdateAvailable('test-type', 'test-version', 'test-description', Date.now().valueOf(), 5566);
  },

  startDownload: function() {
    dump('test start download');
    this._listener.onProgress(10, 100);
  },

  stopDownload: function() {
    dump('test stop download');
  },

  applyUpdate: function() {
    dump('apply update');
  },

  setParameter: function(name, value) {
    dump('set parameter');
    return (name === 'dummy' && value === 'dummy-value');
  },

  getParameter: function(name) {
    dump('get parameter');
    if (name === 'dummy') {
      return 'dummy-value';
    }
  },

  setListener: function(listener) {
    this._listener = listener;
  },

  unsetListener: function() {
    this._listener = null;
  },
};

var factory = {
  createInstance: function(outer, iid) {
    if (outer) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }

    return new TestUpdateProvider().QueryInterface(iid);
  },
  lockFactory: function(aLock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
};

Cm.nsIComponentRegistrar.registerFactory(Components.ID(cid), '', contractId, factory);

var cm = Cc['@mozilla.org/categorymanager;1'].getService(Ci.nsICategoryManager);
cm.addCategoryEntry('system-update-provider', 'DummyProvider',
                    contractId + ',' + cid, false, true);

Cu.import('resource://gre/modules/SystemUpdateService.jsm');
this.SystemUpdateService.addProvider('{17a84227-28f4-453d-9b80-9ae75a5682e0}',
                                     '@mozilla.org/test-update-provider;1',
                                     'DummyProvider');