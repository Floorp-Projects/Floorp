/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

function log(aMsg) {
  //dump("-*- PresentationDeviceInfoManager.js : " + aMsg + "\n");
}

const PRESENTATIONDEVICEINFOMANAGER_CID = Components.ID("{1bd66bef-f643-4be3-b690-0c656353eafd}");
const PRESENTATIONDEVICEINFOMANAGER_CONTRACTID = "@mozilla.org/presentation-device/deviceInfo;1";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function PresentationDeviceInfoManager() {}

PresentationDeviceInfoManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classID: PRESENTATIONDEVICEINFOMANAGER_CID,
  contractID: PRESENTATIONDEVICEINFOMANAGER_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  receiveMessage: function(aMsg) {
    if (!aMsg || !aMsg.data) {
      return;
    }

    let data = aMsg.data;

    log("receive aMsg: " + aMsg.name);
    switch (aMsg.name) {
      case "PresentationDeviceInfoManager:OnDeviceChange": {
        let detail = {
          detail: {
            type: data.type,
            deviceInfo: data.deviceInfo,
          }
        };
        let event = new this._window.CustomEvent("devicechange", Cu.cloneInto(detail, this._window));
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
      }
      case "PresentationDeviceInfoManager:GetAll:Result:Ok": {
        let resolver = this.takePromiseResolver(data.requestId);

        if (!resolver) {
          return;
        }

        resolver.resolve(Cu.cloneInto(data.devices, this._window));
        break;
      }
      case "PresentationDeviceInfoManager:GetAll:Result:Error": {
        let resolver = this.takePromiseResolver(data.requestId);

        if (!resolver) {
          return;
        }

        resolver.reject(data.error);
        break;
      }
    }
  },

  init: function(aWin) {
    log("init");
    this.initDOMRequestHelper(aWin, [
      {name: "PresentationDeviceInfoManager:OnDeviceChange", weakRef: true},
      {name: "PresentationDeviceInfoManager:GetAll:Result:Ok", weakRef: true},
      {name: "PresentationDeviceInfoManager:GetAll:Result:Error", weakRef: true},
    ]);
  },

  uninit: function() {
    log("uninit");
    let self = this;

    this.forEachPromiseResolver(function(aKey) {
      self.takePromiseResolver(aKey).reject("PresentationDeviceInfoManager got destroyed");
    });
  },

  get ondevicechange() {
    return this.__DOM_IMPL__.getEventHandler("ondevicechange");
  },

  set ondevicechange(aHandler) {
    this.__DOM_IMPL__.setEventHandler("ondevicechange", aHandler);
  },

  getAll: function() {
    log("getAll");
    let self = this;
    return this.createPromiseWithId(function(aResolverId) {
      cpmm.sendAsyncMessage("PresentationDeviceInfoManager:GetAll", {
        requestId: aResolverId,
      });
    });
  },

  forceDiscovery: function() {
    cpmm.sendAsyncMessage("PresentationDeviceInfoManager:ForceDiscovery");
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationDeviceInfoManager]);
