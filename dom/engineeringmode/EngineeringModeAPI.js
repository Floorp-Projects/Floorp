/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- EngineeringModeAPI: " + s + "\n");
}


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function EngineeringModeAPI() {
}

EngineeringModeAPI.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classDescription: "Engineering Mode API",
  classID: Components.ID("{27e55b94-fc43-42b3-b0f0-28bebdd804f1}"),
  contractID: "@mozilla.org/dom/engineering-mode-api;1",

  // For DOMRequestHelper: must have nsISupportsWeakReference and nsIObserver.
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  init: function(aWindow) {
    this.initDOMRequestHelper(aWindow, ["EngineeringMode:OnMessage",
                                        "EngineeringMode:SetValue:Result:OK",
                                        "EngineeringMode:SetValue:Result:KO",
                                        "EngineeringMode:GetValue:Result:OK",
                                        "EngineeringMode:GetValue:Result:KO"]);
    cpmm.sendAsyncMessage("EngineeringMode:Register", null);
  },

  uninit: function() {
    cpmm.sendAsyncMessage("EngineeringMode:Unregister", null);
  },

  // This returns a Promise<DOMString>
  getValue: function getValue(aName) {
    debug("getValue " + aName);
    let promiseInit = function(aResolverId) {
      debug("promise init called for getValue " + aName + " has resolverId " + aResolverId);
      cpmm.sendAsyncMessage("EngineeringMode:GetValue", {
        requestId: aResolverId,
        name: aName
      });
    }.bind(this);

    return this.createPromiseWithId(promiseInit);
  },

  // This returns a Promise<void>
  setValue: function setValue(aName, aValue) {
    debug("setValue " + aName + ' as ' + aValue );
    let promiseInit = function(aResolverId) {
      debug("promise init called for getValue " + aName + " has resolverId " + aResolverId);
      cpmm.sendAsyncMessage("EngineeringMode:SetValue", {
        requestId: aResolverId,
        name: aName,
        value: aValue
      });
    }.bind(this);

    return this.createPromiseWithId(promiseInit);
  },

  set onmessage(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onmessage", aHandler);
  },

  get onmessage() {
    return this.__DOM_IMPL__.getEventHandler("onmessage");
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage: name: " + aMessage.name);
    let resolver = null;
    let data = aMessage.data;

    switch (aMessage.name) {
      case "EngineeringMode:OnMessage":
        let detail = Cu.cloneInto(data, this._window);
        let event = new this._window.CustomEvent("message", {"detail": detail});
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
      case "EngineeringMode:GetValue:Result:OK":
      case "EngineeringMode:GetValue:Result:KO":
        resolver = this.takePromiseResolver(data.requestId);
        if (!resolver) {
          return;
        }
        if (aMessage.name === "EngineeringMode:GetValue:Result:OK") {
          resolver.resolve(data.value);
        } else {
          resolver.reject(data.reason);
        }
        break;
      case "EngineeringMode:SetValue:Result:OK":
      case "EngineeringMode:SetValue:Result:KO":
        resolver = this.takePromiseResolver(data.requestId);
        if (!resolver) {
          return;
        }
        if (aMessage.name === "EngineeringMode:SetValue:Result:OK") {
          resolver.resolve();
        } else {
          resolver.reject(data.reason);
        }
        break;
    }
  }

}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EngineeringModeAPI]);
