/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;

function debug(s) {
  dump("-*- KillSwitch.js: " + s + "\n");
}

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

const KILLSWITCH_CID = "{b6eae5c6-971c-4772-89e5-5df626bf3f09}";
const KILLSWITCH_CONTRACTID = "@mozilla.org/moz-kill-switch;1";

const kEnableKillSwitch   = "KillSwitch:Enable";
const kEnableKillSwitchOK = "KillSwitch:Enable:OK";
const kEnableKillSwitchKO = "KillSwitch:Enable:KO";

const kDisableKillSwitch   = "KillSwitch:Disable";
const kDisableKillSwitchOK = "KillSwitch:Disable:OK";
const kDisableKillSwitchKO = "KillSwitch:Disable:KO";

function KillSwitch() {
  this._window       = null;
}

KillSwitch.prototype = {

  __proto__: DOMRequestIpcHelper.prototype,

  init: function(aWindow) {
    DEBUG && debug("init");
    this._window = aWindow;
    this.initDOMRequestHelper(this._window);
  },

  enable: function() {
    DEBUG && debug("KillSwitch: enable");

    cpmm.addMessageListener(kEnableKillSwitchOK, this);
    cpmm.addMessageListener(kEnableKillSwitchKO, this);
    return this.createPromise((aResolve, aReject) => {
      cpmm.sendAsyncMessage(kEnableKillSwitch, {
        requestID: this.getPromiseResolverId({
          resolve: aResolve,
          reject: aReject
        })
      });
    });
  },

  disable: function() {
    DEBUG && debug("KillSwitch: disable");

    cpmm.addMessageListener(kDisableKillSwitchOK, this);
    cpmm.addMessageListener(kDisableKillSwitchKO, this);
    return this.createPromise((aResolve, aReject) => {
      cpmm.sendAsyncMessage(kDisableKillSwitch, {
        requestID: this.getPromiseResolverId({
          resolve: aResolve,
          reject: aReject
        })
      });
    });
  },

  receiveMessage: function(message) {
    DEBUG && debug("Received: " + message.name);

    cpmm.removeMessageListener(kEnableKillSwitchOK, this);
    cpmm.removeMessageListener(kEnableKillSwitchKO, this);
    cpmm.removeMessageListener(kDisableKillSwitchOK, this);
    cpmm.removeMessageListener(kDisableKillSwitchKO, this);

    let req = this.takePromiseResolver(message.data.requestID);

    switch (message.name) {
      case kEnableKillSwitchKO:
      case kDisableKillSwitchKO:
        req.reject(false);
        break;

      case kEnableKillSwitchOK:
      case kDisableKillSwitchOK:
        req.resolve(true);
        break;

      default:
        DEBUG && debug("Unrecognized message: " + message.name);
        break;
    }
  },

  classID : Components.ID(KILLSWITCH_CID),
  contractID : KILLSWITCH_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIKillSwitch,
                                         Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsIObserver,
                                         Ci.nsIMessageListener,
                                         Ci.nsISupportsWeakReference]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([KillSwitch]);
