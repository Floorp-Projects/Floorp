/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

function debug(s) {
  //dump('DEBUG RequestSyncManager: ' + s + '\n');
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/DOMRequestHelper.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function RequestSyncManager() {
  debug('created');
}

RequestSyncManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classDescription: 'RequestSyncManager XPCOM Component',
  classID: Components.ID('{e6f55080-e549-4e30-9d00-15f240fb763c}'),
  contractID: '@mozilla.org/dom/request-sync-manager;1',
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  _messages: [ "RequestSyncManager:Registrations:Return" ],

  init: function(aWindow) {
    debug("init");

    // DOMRequestIpcHelper.initHelper sets this._window
    this.initDOMRequestHelper(aWindow, this._messages);
  },

  sendMessage: function(aMsg, aObj) {
    let self = this;
    return this.createPromise(function(aResolve, aReject) {
      aObj.requestID =
        self.getPromiseResolverId({ resolve: aResolve, reject: aReject });
      cpmm.sendAsyncMessage(aMsg, aObj, null,
                            self._window.document.nodePrincipal);
    });
  },

  registrations: function() {
    debug('registrations');
    return this.sendMessage("RequestSyncManager:Registrations", {});
  },

  receiveMessage: function(aMessage) {
    debug('receiveMessage');

    let req = this.getPromiseResolver(aMessage.data.requestID);
    if (!req) {
      return;
    }

    if ('error' in aMessage.data) {
      req.reject(Cu.cloneInto(aMessage.data.error, this._window));
      return;
    }

    if ('results' in aMessage.data) {
      req.resolve(Cu.cloneInto(aMessage.data.results, this._window));
      return;
    }

    req.resolve();
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RequestSyncManager]);
