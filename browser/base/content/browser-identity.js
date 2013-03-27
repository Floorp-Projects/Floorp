/* -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This JS shim contains the callbacks to fire DOMRequest events for
// navigator.pay API within the payment processor's scope.

'use strict';

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

XPCOMUtils.defineLazyGetter(this, "logger", function() {
  Cu.import('resource://gre/modules/identity/LogUtils.jsm');
  return getLogger("Identity", "toolkit.identity.debug");
});

function IdentityShim() {
  this.isLoaded = false;
}

IdentityShim.prototype = {
  init: function IdentityShim_init() {
    addMessageListener('identity-delegate-watch', this);
    addMessageListener('identity-delegate-request', this);
    addMessageListener('identity-delegate-logout', this);
    sendAsyncMessage('identity-delegate-loaded');
    logger.log('init().  sent identity-delegate-complete');
    this.isLoaded = true;
  },

  uninit: function IdentityShim_uninit() {
    if (this.isLoaded) {
      removeMessageListener('identity-delegate-watch', this);
      removeMessageListener('identity-delegate-request', this);
      removeMessageListener('identity-delegate-logout', this);
      sendAsyncMessage('identity-delegate-complete', null);
      logger.log('uninit().  sent identity-delegate-complete');
      this.isLoaded = false;
    }
  },

  receiveMessage: function IdentityShim_receiveMessage(aMessage) {
    switch (aMessage.name) {
      case 'identity-delegate-watch':
        this.watch(aMessage.json);
        break;
      case 'identity-delegate-request':
        this.request(aMessage.json);
        break;
      case 'identity-delegate-logout':
        this.logout(aMessage.json);
        break;
      default:
        logger.error("received unexpected message:", aMessage.name);
        break;
    }
  },

  _identityDoMethod: function IdentityShim__identityDoMethod(message) {
    sendAsyncMessage('identity-service-doMethod', message);
  },

  _close: function IdentityShim__close() {
    this.uninit();
  },

  watch: function IdentityShim_watch(options) {
    logger.log('doInternalWatch: isLoaded:', this.isLoaded, 'options:', options);
    if (options) {
      let BrowserID = content.wrappedJSObject.BrowserID;
      let callback = function(aParams, aInternalParams) {
        this._identityDoMethod(aParams);
        if (aParams.method === 'ready') {
          this._close();
        }
      }.bind(this);

      BrowserID.internal.watch(
        callback,
        JSON.stringify(options),
        function(...things) {
          logger.log('internal watch returned:', things);
        }
      );
    }
  },

  request: function IdentityShim_request(options) {
    logger.log('doInternalRequest: isLoaded:', this.isLoaded, 'options:', options);
    if (options) {
      var stringifiedOptions = JSON.stringify(options);
      let callback = function(assertion, internalParams) {
        logger.log("received assertion:", assertion);
        internalParams = internalParams || {};
        if (assertion) {
          logger.log("got assertion");
          this._identityDoMethod({
            method: 'login',
            assertion: assertion,
            _internal: options._internal,
            _internalParams: internalParams});
        }
        this._close();
      }.bind(this);

      logger.log('call get() with origin', options.origin, ', callback', callback, ', and options', stringifiedOptions);
      content.wrappedJSObject.BrowserID.internal.get(
        options.origin,
        callback,
        stringifiedOptions
      );
      logger.log('called get()');
    }
  },

  logout: function IdentityShim_logout(options) {
    logger.log('doInternalLogout: isLoaded:', this.isLoaded, 'options:', options);
    if (options) {
      let BrowserID = content.wrappedJSObject.BrowserID;
      let callback = function() {
        this._identityDoMethod({method: 'logout', _internal: options._internal});
        this._close();
      }.bind(this);

      BrowserID.internal.logout(options.origin, callback);
    }
  }
};

this.shim = null;

addEventListener('DOMContentLoaded', function(e) {
  content.addEventListener('load', function(e) {
    logger.log('content loaded');
    this.shim = new IdentityShim();
    this.shim.init();
  });
});

content.addEventListener('beforeunload', function(e) {
  if (this.shim) {
    this.shim.uninit();
  }
});


