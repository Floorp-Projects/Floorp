/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

let debug = Services.prefs.getBoolPref("dom.system_update.debug")
              ? (aMsg) => dump("-*- SystemUpdateManager.js : " + aMsg + "\n")
              : (aMsg) => {};

const SYSTEMUPDATEPROVIDER_CID = Components.ID("{11fbea3d-fd94-459a-b8fb-557fe19e473a}");
const SYSTEMUPDATEMANAGER_CID = Components.ID("{e8530001-ba5b-46ab-a306-7fbeb692d0fe}");
const SYSTEMUPDATEMANAGER_CONTRACTID = "@mozilla.org/system-update-manager;1";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function SystemUpdateProvider(win, provider) {
  this.initDOMRequestHelper(win, [
    {name: "SystemUpdate:OnUpdateAvailable", weakRef: true},
    {name: "SystemUpdate:OnProgress", weakRef: true},
    {name: "SystemUpdate:OnUpdateReady", weakRef: true},
    {name: "SystemUpdate:OnError", weakRef: true},
  ]);
  this._provider = Cu.cloneInto(provider, win);
}

SystemUpdateProvider.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classID: SYSTEMUPDATEPROVIDER_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  receiveMessage: function(aMsg) {
    if (!aMsg || !aMsg.json) {
      return;
    }

    let json = aMsg.json;

    if (json.uuid !== this._provider.uuid) {
      return;
    }

    debug("receive msg: " + aMsg.name);
    switch (aMsg.name) {
      case "SystemUpdate:OnUpdateAvailable": {
        let detail = {
          detail: {
            packageInfo: json.packageInfo
          }
        };
        let event = new this._window.CustomEvent("updateavailable",
                                                  Cu.cloneInto(detail, this._window));
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
      }
      case "SystemUpdate:OnProgress": {
        let event = new this._window.ProgressEvent("progress", {lengthComputable: true,
                                                                loaded: json.loaded,
                                                                total: json.total});
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
      }
      case "SystemUpdate:OnUpdateReady": {
        let event = new this._window.Event("updateready");
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
      }
      case "SystemUpdate:OnError": {
        let event = new this._window.ErrorEvent("error", {message: json.message});
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
      }
    }
  },

  destroy: function() {
    this.destroyDOMRequestHelper();
  },

  get name() {
    return this._provider.name;
  },

  get uuid() {
    return this._provider.uuid;
  },

  get onupdateavailable() {
    return this.__DOM_IMPL__.getEventHandler("onupdateavailable");
  },
  set onupdateavailable(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onupdateavailable", aHandler);
  },
  get onprogress() {
    return this.__DOM_IMPL__.getEventHandler("onprogress");
  },
  set onprogress(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onprogress", aHandler);
  },
  get onupdateready() {
    return this.__DOM_IMPL__.getEventHandler("onupdateready");
  },
  set onupdateready(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onupdateready", aHandler);
  },
  get onerror() {
    return this.__DOM_IMPL__.getEventHandler("onerror");
  },
  set onerror(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onerror", aHandler);
  },

  checkForUpdate: function() {
    let self = this;
    cpmm.sendAsyncMessage("SystemUpdate:CheckForUpdate", {
      uuid: self._provider.uuid
    });
  },
  startDownload: function() {
    let self = this;
    cpmm.sendAsyncMessage("SystemUpdate:StartDownload", {
      uuid: self._provider.uuid
    });
  },
  stopDownload: function() {
    let self = this;
    cpmm.sendAsyncMessage("SystemUpdate:StopDownload", {
      uuid: self._provider.uuid
    });
  },
  applyUpdate: function() {
    let self = this;
    cpmm.sendAsyncMessage("SystemUpdate:ApplyUpdate", {
      uuid: self._provider.uuid
    });
  },
  setParameter: function(aName, aValue) {
    let self = this;
    return cpmm.sendSyncMessage("SystemUpdate:SetParameter", {
      uuid: self._provider.uuid,
      name: aName,
      value: aValue
    })[0];
  },
  getParameter: function(aName) {
    let self = this;
    return cpmm.sendSyncMessage("SystemUpdate:GetParameter", {
      uuid: self._provider.uuid,
      name: aName
    })[0];
  },
};

function SystemUpdateManager() {}

SystemUpdateManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classID: SYSTEMUPDATEMANAGER_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  receiveMessage: function(aMsg) {
    if (!aMsg || !aMsg.json) {
      return;
    }

    let json = aMsg.json;
    let resolver = this.takePromiseResolver(json.requestId);

    if (!resolver) {
      return;
    }

    debug("receive msg: " + aMsg.name);
    switch (aMsg.name) {
      case "SystemUpdate:GetProviders:Result:OK": {
        resolver.resolve(Cu.cloneInto(json.providers, this._window));
        break;
      }
      case "SystemUpdate:SetActiveProvider:Result:OK":
      case "SystemUpdate:GetActiveProvider:Result:OK": {
        let updateProvider = new SystemUpdateProvider(this._window, json.provider);
        resolver.resolve(this._window.SystemUpdateProvider._create(this._window,
                                                                   updateProvider));
        break;
      }
      case "SystemUpdate:GetProviders:Result:Error":
      case "SystemUpdate:GetActiveProvider:Result:Error":
      case "SystemUpdate:SetActiveProvider:Result:Error": {
        resolver.reject(json.error);
        break;
      }
    }
  },

  init: function(aWindow) {
    this.initDOMRequestHelper(aWindow, [
      {name: "SystemUpdate:GetProviders:Result:OK", weakRef: true},
      {name: "SystemUpdate:GetProviders:Result:Error", weakRef: true},
      {name: "SystemUpdate:GetActiveProvider:Result:OK", weakRef: true},
      {name: "SystemUpdate:GetActiveProvider:Result:Error", weakRef: true},
      {name: "SystemUpdate:SetActiveProvider:Result:OK", weakRef: true},
      {name: "SystemUpdate:SetActiveProvider:Result:Error", weakRef: true},
    ]);
  },

  uninit: function() {
    let self = this;

    this.forEachPromiseResolver(function(aKey) {
      self.takePromiseResolver(aKey).reject("SystemUpdateManager got destroyed");
    });
  },

  getProviders: function() {
    return this._sendPromise(function(aResolverId) {
      cpmm.sendAsyncMessage("SystemUpdate:GetProviders", {
        requestId: aResolverId,
      });
    });
  },

  getActiveProvider: function() {
    return this._sendPromise(function(aResolverId) {
      cpmm.sendAsyncMessage("SystemUpdate:GetActiveProvider", {
        requestId: aResolverId,
      });
    });
  },

  setActiveProvider: function(aUuid) {
    return this._sendPromise(function(aResolverId) {
      cpmm.sendAsyncMessage("SystemUpdate:SetActiveProvider", {
        requestId: aResolverId,
        uuid: aUuid
      });
    });
  },

  _sendPromise: function(aCallback) {
    let self = this;
    return this.createPromise(function(aResolve, aReject) {
      let resolverId = self.getPromiseResolverId({resolve: aResolve,
                                                  reject: aReject});
      aCallback(resolverId);
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SystemUpdateManager]);
