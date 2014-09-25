/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- EngineeringModeService: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "EngineeringMode",
                                   "@mozilla.org/b2g/engineering-mode-impl;1",
                                   "nsIEngineeringMode");

function EngineeringModeService() {
}

EngineeringModeService.prototype = {
  classID: Components.ID("{1345a96a-7b8d-4017-a776-07d918f14d84}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIEngineeringModeMessageHandler]),

  observe: function(aSubject, aTopic, aData) {
    debug("-- init");

    switch(aTopic) {
      case "profile-after-change":
        Services.obs.addObserver(this, "xpcom-shutdown", false);
        ppmm.addMessageListener("EngineeringMode:Register", this);
        ppmm.addMessageListener("EngineeringMode:Unregister", this);
        ppmm.addMessageListener("EngineeringMode:SetValue", this);
        ppmm.addMessageListener("EngineeringMode:GetValue", this);
        this._clients = [];
        break;
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        ppmm.removeMessageListener("EngineeringMode:Register", this);
        ppmm.removeMessageListener("EngineeringMode:Unregister", this);
        ppmm.removeMessageListener("EngineeringMode:SetValue", this);
        ppmm.removeMessageListener("EngineeringMode:GetValue", this);
        if (this._hasEngineeringModeImpl()) {
          EngineeringMode.setMessageHandler(function(){});
        }
        this._clients = null;
        break;
    }
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage: name: " + aMessage.name);

    if (!aMessage.target.assertPermission("engineering-mode")) {
      debug(aMessage.name + " from a content process with no 'engineering-mode' privileges.");
      return;
    }

    switch (aMessage.name) {
      case "EngineeringMode:Register":
        // Lazy bind message handler until we have first client.
        if (this._hasEngineeringModeImpl() && this._clients.length === 0) {
          EngineeringMode.setMessageHandler(this.onMessage.bind(this));
        }

        this._clients.push(aMessage.target);
        break;

      case "EngineeringMode:Unregister":
        let index = this._clients.indexOf(aMessage.target);
        if (index > -1) {
          this._clients.splice(index, 1);
        }
        break;

      case "EngineeringMode:SetValue":
        this.setValue(aMessage.target, aMessage.data);
        break;

      case "EngineeringMode:GetValue":
        this.getValue(aMessage.target, aMessage.data);
        break;
    }
  },

  setValue: function(aTarget, aData) {
    if (!this._hasEngineeringModeImpl()) {
      aTarget.sendAsyncMessage("EngineeringMode:SetValue:Result:KO", {
        requestId: aData.requestId,
        reason: "No engineering mode implementation"
      });
      return;
    }

    EngineeringMode.setValue(aData.name, aData.value, {
      onsuccess: function() {
        aTarget.sendAsyncMessage("EngineeringMode:SetValue:Result:OK", {
          requestId: aData.requestId
        });
      },
      onerror: function(aError) {
        aTarget.sendAsyncMessage("EngineeringMode:SetValue:Result:KO", {
          requestId: aData.requestId,
          reason: "Error: code " + aError
        });
      }
    });
  },

  getValue: function(aTarget, aData) {
    if (!this._hasEngineeringModeImpl()) {
      aTarget.sendAsyncMessage("EngineeringMode:GetValue:Result:KO", {
        requestId: aData.requestId,
        reason: "No engineering mode implementation"
      });
      return;
    }

    EngineeringMode.getValue(aData.name, {
      onsuccess: function(aValue) {
        aTarget.sendAsyncMessage("EngineeringMode:GetValue:Result:OK", {
          requestId: aData.requestId,
          value: aValue
        });
      },
      onerror: function(aError) {
        aTarget.sendAsyncMessage("EngineeringMode:GetValue:Result:KO", {
          requestId: aData.requestId,
          reason: "Error: code " + aError
        });
      }
    });
  },

  onMessage: function(aMessage) {
    this._clients.forEach(function(aClient) {
      aClient.sendAsyncMessage("EngineeringMode:OnMessage", {
        data: aMessage
      });
    });
  },

  _hasEngineeringModeImpl: function() {
    if (typeof Cc["@mozilla.org/b2g/engineering-mode-impl;1"] === "undefined") {
      debug("Can not get engineering mode implementation.");
      return false;
    }
    return true;
  }

}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EngineeringModeService]);
