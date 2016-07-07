/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SystemAppProxy.jsm");

function debug(aMsg) {
  dump("-- SystemMessageInternal " + Date.now() + " : " + aMsg + "\n");
}

// Implementation of the component used by internal users.

function SystemMessageInternal() {
}

SystemMessageInternal.prototype = {

  sendMessage: function(aType, aMessage, aPageURI, aManifestURI, aExtra) {
    debug(`sendMessage ${aType} ${aMessage} ${aPageURI} ${aExtra}`);
    SystemAppProxy._sendCustomEvent("mozSystemMessage", {
      action: "send",
      type: aType,
      message: aMessage,
      pageURI: aPageURI,
      extra: aExtra
    });
    return Promise.resolve();
  },

  broadcastMessage: function(aType, aMessage, aExtra) {
    debug(`broadcastMessage ${aType} ${aMessage} ${aExtra}`);
    SystemAppProxy._sendCustomEvent("mozSystemMessage", {
      action: "broadcast",
      type: aType,
      message: aMessage,
      extra: aExtra
    });
    return Promise.resolve();
  },

  registerPage: function(aType, aPageURI, aManifestURI) {
    SystemAppProxy._sendCustomEvent("mozSystemMessage", {
      action: "register",
      type: aType,
      pageURI: aPageURI
    });
    debug(`registerPage ${aType} ${aPageURI} ${aManifestURI}`);
  },

  classID: Components.ID("{70589ca5-91ac-4b9e-b839-d6a88167d714}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesInternal])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemMessageInternal]);
