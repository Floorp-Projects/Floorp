/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["SystemUpdateService"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const CATEGORY_SYSTEM_UPDATE_PROVIDER = "system-update-provider";
const PROVIDER_ACTIVITY_IDLE = 0;
const PROVIDER_ACTIVITY_CHECKING = 1;
const PROVIDER_ACTIVITY_DOWNLOADING = 1 << 1;
const PROVIDER_ACTIVITY_APPLYING = 1 << 2;

let debug = Services.prefs.getBoolPref("dom.system_update.debug")
              ? (aMsg) => dump("-*- SystemUpdateService.jsm : " + aMsg + "\n")
              : (aMsg) => {};

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

function ActiveProvider(aProvider) {
  this.id = aProvider.id;
  this._instance = Cc[aProvider.contractId].getService(Ci.nsISystemUpdateProvider);
  this._instance.setListener(this);
}

ActiveProvider.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemUpdateListener]),

  _activity: PROVIDER_ACTIVITY_IDLE,

  destroy: function() {
    if (this._instance) {
      this._instance.unsetListener();
      this._instance = null;
    }

    this.id = null;
  },

  checkForUpdate: function() {
    this._execFuncIfNotInActivity(PROVIDER_ACTIVITY_CHECKING,
                                  this._instance.checkForUpdate);
  },

  startDownload: function() {
    this._execFuncIfNotInActivity(PROVIDER_ACTIVITY_DOWNLOADING,
                                  this._instance.startDownload);
  },

  stopDownload: function() {
    this._execFuncIfNotInActivity(PROVIDER_ACTIVITY_DOWNLOADING,
                                  this._instance.stopDownload);
  },

  applyUpdate: function() {
    this._execFuncIfNotInActivity(PROVIDER_ACTIVITY_APPLYING,
                                  this._instance.applyUpdate);
  },

  setParameter: function(aName, aValue) {
    return this._instance.setParameter(aName, aValue);
  },

  getParameter: function(aName) {
    return this._instance.getParameter(aName);
  },

  // nsISystemUpdateListener
  onUpdateAvailable: function(aType, aVersion, aDescription, aBuildDate, aSize) {
    this._execFuncIfActiveAndInAction(PROVIDER_ACTIVITY_CHECKING, function() {
      ppmm.broadcastAsyncMessage("SystemUpdate:OnUpdateAvailable", {
        uuid: this.id,
        packageInfo: {
          type: aType,
          version: aVersion,
          description: aDescription,
          buildDate: aBuildDate,
          size: aSize,
        }
      });

      this._unsetActivity(PROVIDER_ACTIVITY_CHECKING);
    }.bind(this));
  },

  onProgress: function(aLoaded, aTotal) {
    this._execFuncIfActiveAndInAction(PROVIDER_ACTIVITY_DOWNLOADING, function() {
      ppmm.broadcastAsyncMessage("SystemUpdate:OnProgress", {
        uuid: this.id,
        loaded: aLoaded,
        total: aTotal,
      });
    }.bind(this));
  },

  onUpdateReady: function() {
    this._execFuncIfActiveAndInAction(PROVIDER_ACTIVITY_DOWNLOADING, function() {
      ppmm.broadcastAsyncMessage("SystemUpdate:OnUpdateReady", {
        uuid: this.id,
      });

      this._unsetActivity(PROVIDER_ACTIVITY_DOWNLOADING);
    }.bind(this));
  },

  onError: function(aErrMsg) {
    if (!SystemUpdateService._isActiveProviderId(this.id)) {
      return;
    }

    ppmm.broadcastAsyncMessage("SystemUpdate:OnError", {
      uuid: this.id,
      message: aErrMsg,
    });

    this._activity = PROVIDER_ACTIVITY_IDLE;
  },

  isIdle: function() {
    return this._activity === PROVIDER_ACTIVITY_IDLE;
  },

  _isInActivity: function(aActivity) {
    return (this._activity & aActivity) !== PROVIDER_ACTIVITY_IDLE;
  },

  _setActivity: function(aActivity) {
    this._activity |= aActivity;
  },

  _unsetActivity: function(aActivity) {
    this._activity &= ~aActivity;
  },

  _execFuncIfNotInActivity: function(aActivity, aFunc) {
    if (!this._isInActivity(aActivity)) {
      this._setActivity(aActivity);
      aFunc();
    }
  },

  _execFuncIfActiveAndInAction: function(aActivity, aFunc) {
    if (!SystemUpdateService._isActiveProviderId(this.id)) {
      return;
    }
    if (this._isInActivity(aActivity)) {
      aFunc();
    }
  },

};

this.SystemUpdateService = {
  _providers: [],
  _activeProvider: null,

  _updateActiveProvider: function(aProvider) {
    if (this._activeProvider) {
      this._activeProvider.destroy();
    }

    this._activeProvider = new ActiveProvider(aProvider);
  },

  _isActiveProviderId: function(aId) {
    return (this._activeProvider && this._activeProvider.id === aId);
  },

  init: function() {
    debug("init");

    let messages = ["SystemUpdate:GetProviders",
                    "SystemUpdate:GetActiveProvider",
                    "SystemUpdate:SetActiveProvider",
                    "SystemUpdate:CheckForUpdate",
                    "SystemUpdate:StartDownload",
                    "SystemUpdate:StopDownload",
                    "SystemUpdate:ApplyUpdate",
                    "SystemUpdate:SetParameter",
                    "SystemUpdate:GetParameter"];
    messages.forEach((function(aMsgName) {
      ppmm.addMessageListener(aMsgName, this);
    }).bind(this));

    // load available provider list
    let catMan = Cc["@mozilla.org/categorymanager;1"].getService(Ci.nsICategoryManager);
    let entries = catMan.enumerateCategory(CATEGORY_SYSTEM_UPDATE_PROVIDER);
    while (entries.hasMoreElements()) {
      let name = entries.getNext().QueryInterface(Ci.nsISupportsCString).data;
      let [contractId, id] = catMan.getCategoryEntry(CATEGORY_SYSTEM_UPDATE_PROVIDER, name).split(",");
      this._providers.push({
        id: id,
        name: name,
        contractId: contractId
      });
    }
    debug("available providers: " + JSON.stringify(this._providers));

    // setup default active provider
    let defaultActive;
    try {
      defaultActive = Services.prefs.getCharPref("dom.system_update.active");
    } catch (e) {}

    if (defaultActive) {
      let defaultProvider = this._providers.find(function(aProvider) {
        return aProvider.contractId === defaultActive;
      });

      if (defaultProvider) {
        this._updateActiveProvider(defaultProvider);
      }
    }
  },

  addProvider: function(aClassId, aContractId, aName) {
    debug("addProvider");

    //did not allow null or empty string to add.
    if(!aClassId || !aContractId || !aName) {
      return;
    }

    let existedProvider = this._providers.find(function(provider) {
      return provider.id === aClassId;
    });

    //skip if adding the existed provider.
    if (existedProvider) {
      debug("existing providers: " + JSON.stringify(existedProvider));
      return;
    }

    //dynamically add the provider info to list.
    this._providers.push({
        id: aClassId,
        name: aName,
        contractId: aContractId
    });
    debug("available providers: " + JSON.stringify(this._providers));
  },

  getProviders: function(aData, aMm) {
    debug("getProviders");

    aData.providers = [];
    for (let provider of this._providers) {
      aData.providers.push({
        name: provider.name,
        uuid: provider.id
      });
    }
    aMm.sendAsyncMessage("SystemUpdate:GetProviders:Result:OK", aData);
  },

  getActiveProvider: function(aData, aMm) {
    debug("getActiveProvider");

    let self = this;
    let providerInfo = this._providers.find(function(provider) {
      return self._isActiveProviderId(provider.id);
    });

    if (!providerInfo) {
      aData.error = "NotFoundError";
      aMm.sendAsyncMessage("SystemUpdate:GetActiveProvider:Result:Error", aData);
      return;
    }

    aData.provider = {
      name: providerInfo.name,
      uuid: providerInfo.id
    };
    aMm.sendAsyncMessage("SystemUpdate:GetActiveProvider:Result:OK", aData);
  },

  setActiveProvider: function(aData, aMm) {
    debug("setActiveProvider");

    let self = this;
    let selectedProvider = this._providers.find(function(provider) {
      return provider.id === aData.uuid;
    });

    if (!selectedProvider) {
      aData.error = "DataError";
      aMm.sendAsyncMessage("SystemUpdate:SetActiveProvider:Result:Error", aData);
      return;
    }

    if (!this._isActiveProviderId(selectedProvider.id)) {
      // not allow changing active provider while there is an ongoing update activity
      if (this.activeProvider && !this._activeProvider.isIdle()) {
        aData.error = "DataError";
        aMm.sendAsyncMessage("SystemUpdate:SetActiveProvider:Result:Error", aData);
        return;
      }

      this._updateActiveProvider(selectedProvider);
      Services.prefs.setCharPref("dom.system_update.active", selectedProvider.contractId);
    }

    aData.provider = {
      name: selectedProvider.name,
      uuid: selectedProvider.id
    };
    aMm.sendAsyncMessage("SystemUpdate:SetActiveProvider:Result:OK", aData);
  },

  receiveMessage: function(aMessage) {
    if (!aMessage.target.assertPermission("system-update")) {
      debug("receive message " + aMessage.name +
            " from a content process with no 'system-update' privileges.");
      return null;
    }

    let msg = aMessage.data || {};
    let mm = aMessage.target;

    switch (aMessage.name) {
      case "SystemUpdate:GetProviders": {
        this.getProviders(msg, mm);
        break;
      }
      case "SystemUpdate:GetActiveProvider": {
        this.getActiveProvider(msg, mm);
        break;
      }
      case "SystemUpdate:SetActiveProvider": {
        this.setActiveProvider(msg, mm);
        break;
      }
      case "SystemUpdate:CheckForUpdate": {
        if (this._isActiveProviderId(msg.uuid)) {
          this._activeProvider.checkForUpdate();
        }
        break;
      }
      case "SystemUpdate:StartDownload": {
        if (this._isActiveProviderId(msg.uuid)) {
          this._activeProvider.startDownload();
        }
        break;
      }
      case "SystemUpdate:StopDownload": {
        if (this._isActiveProviderId(msg.uuid)) {
          this._activeProvider.stopDownload();
        }
        break;
      }
      case "SystemUpdate:ApplyUpdate": {
        if (this._isActiveProviderId(msg.uuid)) {
          this._activeProvider.applyUpdate();
        }
        break;
      }
      case "SystemUpdate:SetParameter": {
        if (this._isActiveProviderId(msg.uuid)) {
          return this._activeProvider.setParameter(msg.name, msg.value);
        }
        break;
      }
      case "SystemUpdate:GetParameter": {
        if (this._isActiveProviderId(msg.uuid)) {
          return this._activeProvider.getParameter(msg.name);
        }
        break;
      }
    }
  },

};

SystemUpdateService.init();
