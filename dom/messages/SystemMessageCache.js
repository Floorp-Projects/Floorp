/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function debug(aMsg) {
   // dump("-- SystemMessageCache " + Date.now() + " : " + aMsg + "\n");
}

const kMessages = ["SystemMessageCache:RefreshCache"];

function SystemMessageCache() {
  debug("init");

  this._pagesCache = [];

  dump("SystemMessageCache: init");
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  kMessages.forEach(function(aMessage) {
    cpmm.addMessageListener(aMessage, this);
  }, this);
}

SystemMessageCache.prototype = {

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "xpcom-shutdown":
      debug("received xpcom-shutdown");
      kMessages.forEach(function(aMessage) {
        cpmm.removeMessageListener(aMessage, this);
      }, this);
      Services.obs.removeObserver(this, "xpcom-shutdown");
      cpmm = null;
      break;
    default:
      break;
    }
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
    case "SystemMessageCache:RefreshCache":
      this._pagesCache = aMessage.data;
      debug("received RefreshCache");
      break;
    default:
      debug("received unknown message " + aMessage.name);
      break;
    }
  },

  hasPendingMessage: function(aType, aPageURL, aManifestURL) {
    let hasMessage = this._pagesCache.some(function(aPage) {
      if (aPage.type === aType &&
          aPage.pageURL === aPageURL &&
          aPage.manifestURL === aManifestURL) {
        return true;
      }
      return false;
    }, this);
    debug("hasPendingMessage " + aType + " " + aPageURL + " " +
          aManifestURL + ": " + hasMessage);
    return hasMessage;
  },

  classID: Components.ID("{5a19d86a-21e5-4ac8-9634-8c364c73f87f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessageCache,
                                         Ci.nsIMessageListener,
                                         Ci.nsIObserver])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageCache]);
