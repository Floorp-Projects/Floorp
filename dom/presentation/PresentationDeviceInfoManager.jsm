/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["PresentationDeviceInfoService"];

function log(aMsg) {
  //dump("PresentationDeviceInfoManager.jsm: " + aMsg + "\n");
}

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "presentationDeviceManager",
                                   "@mozilla.org/presentation-device/manager;1",
                                   "nsIPresentationDeviceManager");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

this.PresentationDeviceInfoService = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                         Ci.nsIObserver]),

  init: function() {
    log("init");
    ppmm.addMessageListener("PresentationDeviceInfoManager:GetAll", this);
    ppmm.addMessageListener("PresentationDeviceInfoManager:ForceDiscovery", this);
    Services.obs.addObserver(this, "presentation-device-change", false);
  },

  getAll: function(aData, aMm) {
    log("getAll");
    let deviceArray = presentationDeviceManager.getAvailableDevices().QueryInterface(Ci.nsIArray);
    if (!deviceArray) {
      aData.error = "DataError";
      aMm.sendAsyncMessage("PresentationDeviceInfoManager:GetAll:Result:Error", aData);
      return;
    }

    aData.devices = [];
    for (let i = 0; i < deviceArray.length; i++) {
      let device = deviceArray.queryElementAt(i, Ci.nsIPresentationDevice);
      aData.devices.push({
        id: device.id,
        name: device.name,
        type: device.type,
      });
    }
    aMm.sendAsyncMessage("PresentationDeviceInfoManager:GetAll:Result:Ok", aData);
  },

  forceDiscovery: function() {
    log("forceDiscovery");
    presentationDeviceManager.forceDiscovery();
  },

  observe: function(aSubject, aTopic, aData) {
    log("observe: " + aTopic);

    let device = aSubject.QueryInterface(Ci.nsIPresentationDevice);
    let data = {
      type: aData,
      deviceInfo: {
        id: device.id,
        name: device.name,
        type: device.type,
      },
    };
    ppmm.broadcastAsyncMessage("PresentationDeviceInfoManager:OnDeviceChange", data);
  },

  receiveMessage: function(aMessage) {
    if (!aMessage.target.assertPermission("presentation-device-manage")) {
      debug("receive message " + aMessage.name +
            " from a content process with no 'presentation-device-manage' privileges.");
      return null;
    }

    let msg = aMessage.data || {};
    let mm = aMessage.target;

    log("receiveMessage: " + aMessage.name);
    switch (aMessage.name) {
      case "PresentationDeviceInfoManager:GetAll": {
        this.getAll(msg, mm);
        break;
      }
      case "PresentationDeviceInfoManager:ForceDiscovery": {
        this.forceDiscovery();
        break;
      }
    }
  },
};

this.PresentationDeviceInfoService.init();
