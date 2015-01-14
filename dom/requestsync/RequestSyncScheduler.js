/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

function debug(s) {
  //dump('DEBUG RequestSyncScheduler: ' + s + '\n');
}

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/DOMRequestHelper.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyServiceGetter(this, 'cpmm',
                                   '@mozilla.org/childprocessmessagemanager;1',
                                   'nsIMessageSender');

function RequestSyncScheduler() {
  debug('created');
}

RequestSyncScheduler.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classDescription: 'RequestSyncScheduler XPCOM Component',
  classID: Components.ID('{8ee5ab74-15c4-478f-9d32-67627b9f0f1a}'),
  contractID: '@mozilla.org/dom/request-sync-scheduler;1',
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  _messages: [ 'RequestSync:Register:Return',
               'RequestSync:Unregister:Return',
               'RequestSync:Registrations:Return',
               'RequestSync:Registration:Return' ],

  init: function(aWindow) {
    debug('init');

    // DOMRequestIpcHelper.initHelper sets this._window
    this.initDOMRequestHelper(aWindow, this._messages);
  },

  register: function(aTask, aParams) {
    debug('register');
    return this.sendMessage('RequestSync:Register',
                            { task: aTask, params: aParams });
  },

  unregister: function(aTask) {
    debug('unregister');
    return this.sendMessage('RequestSync:Unregister',
                            { task: aTask });
  },

  registrations: function() {
    debug('registrations');
    return this.sendMessage('RequestSync:Registrations', {});
  },

  registration: function(aTask) {
    debug('registration');
    return this.sendMessage('RequestSync:Registration',
                            { task: aTask });
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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RequestSyncScheduler]);
