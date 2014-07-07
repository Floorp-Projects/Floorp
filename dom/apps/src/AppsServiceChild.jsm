/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

// This module exposes a subset of the functionalities of the parent DOM
// Registry to content processes, to be used from the AppsService component.

this.EXPORTED_SYMBOLS = ["DOMApplicationRegistry", "WrappedManifestCache"];

Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function debug(s) {
  //dump("-*- AppsServiceChild.jsm: " + s + "\n");
}

const APPS_IPC_MSG_NAMES = [
  "Webapps:AddApp",
  "Webapps:RemoveApp",
  "Webapps:UpdateApp",
  "Webapps:CheckForUpdate:Return:KO",
  "Webapps:FireEvent",
  "Webapps:UpdateState"
];

// A simple cache for the wrapped manifests.
this.WrappedManifestCache = {
  _cache: { },

  // Gets an entry from the cache, and populates the cache if needed.
  get: function mcache_get(aManifestURL, aManifest, aWindow, aInnerWindowID) {
    if (!aManifest) {
      return;
    }

    if (!(aManifestURL in this._cache)) {
      this._cache[aManifestURL] = { };
    }

    let winObjs = this._cache[aManifestURL];
    if (!(aInnerWindowID in winObjs)) {
      winObjs[aInnerWindowID] = Cu.cloneInto(aManifest, aWindow);
    }

    return winObjs[aInnerWindowID];
  },

  // Invalidates an entry in the cache.
  evict: function mcache_evict(aManifestURL, aInnerWindowID) {
    debug("Evicting manifest " + aManifestURL + " window ID " +
          aInnerWindowID);
    if (aManifestURL in this._cache) {
      let winObjs = this._cache[aManifestURL];
      if (aInnerWindowID in winObjs) {
        delete winObjs[aInnerWindowID];
      }

      if (Object.keys(winObjs).length == 0) {
        delete this._cache[aManifestURL];
      }
    }
  },

  observe: function(aSubject, aTopic, aData) {
    // Clear the cache on memory pressure.
    this._cache = { };
    Cu.forceGC();
  },

  init: function() {
    Services.obs.addObserver(this, "memory-pressure", false);
  }
};

this.WrappedManifestCache.init();


// DOMApplicationRegistry keeps a cache containing a list of apps in the device.
// This information is updated with the data received from the main process and
// it is queried by the DOM objects to set their state.
// This module handle all the messages broadcasted from the parent process,
// including DOM events, which are dispatched to the corresponding DOM objects.

this.DOMApplicationRegistry = {
  // DOMApps will hold a list of arrays of weak references to
  // mozIDOMApplication objects indexed by manifest URL.
  DOMApps: {},

  ready: false,
  webapps: null,

  init: function init() {
    this.cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                  .getService(Ci.nsISyncMessageSender);

    APPS_IPC_MSG_NAMES.forEach((function(aMsgName) {
      this.cpmm.addMessageListener(aMsgName, this);
    }).bind(this));

    this.cpmm.sendAsyncMessage("Webapps:RegisterForMessages", {
      messages: APPS_IPC_MSG_NAMES
    });

    // We need to prime the cache with the list of apps.
    let list = this.cpmm.sendSyncMessage("Webapps:GetList", { })[0];
    this.webapps = list.webapps;
    // We need a fast mapping from localId -> app, so we add an index.
    // We also add the manifest to the app object.
    this.localIdIndex = { };
    for (let id in this.webapps) {
      let app = this.webapps[id];
      this.localIdIndex[app.localId] = app;
      app.manifest = list.manifests[id];
    }

    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function(aSubject, aTopic, aData) {
    // cpmm.addMessageListener causes the DOMApplicationRegistry object to
    // live forever if we don't clean up properly.
    this.webapps = null;
    this.DOMApps = null;

    APPS_IPC_MSG_NAMES.forEach((aMsgName) => {
      this.cpmm.removeMessageListener(aMsgName, this);
    });

    this.cpmm.sendAsyncMessage("Webapps:UnregisterForMessages",
                               APPS_IPC_MSG_NAMES)
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("Received " + aMessage.name + " message.");
    let msg = aMessage.data;
    switch (aMessage.name) {
      case "Webapps:AddApp":
        this.webapps[msg.id] = msg.app;
        this.localIdIndex[msg.app.localId] = msg.app;
        if (msg.manifest) {
          this.webapps[msg.id].manifest = msg.manifest;
        }
        break;
      case "Webapps:RemoveApp":
        delete this.DOMApps[this.webapps[msg.id].manifestURL];
        delete this.localIdIndex[this.webapps[msg.id].localId];
        delete this.webapps[msg.id];
        break;
      case "Webapps:UpdateApp":
        let app = this.webapps[msg.oldId];
        if (!app) {
          return;
        }

        if (msg.app) {
          for (let prop in msg.app) {
            app[prop] = msg.app[prop];
          }
        }

        this.webapps[msg.newId] = app;
        this.localIdIndex[app.localId] = app;
        delete this.webapps[msg.oldId];

        let apps = this.DOMApps[msg.app.manifestURL];
        if (!apps) {
          return;
        }
        for (let i = 0; i < apps.length; i++) {
          let domApp = apps[i].get();
          if (!domApp) {
            apps.splice(i);
            continue;
          }
          domApp._proxy = new Proxy(domApp, {
            get: function(target, prop) {
              if (!DOMApplicationRegistry.webapps[msg.newId]) {
                return;
              }
              return DOMApplicationRegistry.webapps[msg.newId][prop];
            },
            set: function(target, prop, val) {
              if (!DOMApplicationRegistry.webapps[msg.newId]) {
                return;
              }
              DOMApplicationRegistry.webapps[msg.newId][prop] = val;
              return;
            },
          });
        }
        break;
      case "Webapps:FireEvent":
        this._fireEvent(aMessage);
        break;
      case "Webapps:UpdateState":
        this._updateState(msg);
        break;
      case "Webapps:CheckForUpdate:Return:KO":
        let DOMApps = this.DOMApps[msg.manifestURL];
        if (!DOMApps || !msg.requestID) {
          return;
        }
        DOMApps.forEach((DOMApp) => {
          let domApp = DOMApp.get();
          if (domApp && msg.requestID) {
            domApp._fireRequestResult(aMessage, true /* aIsError */);
          }
        });
        break;
    }
  },

  /**
   * mozIDOMApplication management
   */

  // Every time a DOM app is created, we save a weak reference to it that will
  // be used to dispatch events and fire request results.
  addDOMApp: function(aApp, aManifestURL, aId) {
    let weakRef = Cu.getWeakReference(aApp);

    if (!this.DOMApps[aManifestURL]) {
      this.DOMApps[aManifestURL] = [];
    }

    let apps = this.DOMApps[aManifestURL];

    // Get rid of dead weak references.
    for (let i = 0; i < apps.length; i++) {
      if (!apps[i].get()) {
        apps.splice(i);
      }
    }

    apps.push(weakRef);

    // Each DOM app contains a proxy object used to build their state. We
    // return the handler for this proxy object with traps to get and set
    // app properties kept in the DOMApplicationRegistry app cache.
    return {
      get: function(target, prop) {
        if (!DOMApplicationRegistry.webapps[aId]) {
          return;
        }
        return DOMApplicationRegistry.webapps[aId][prop];
      },
      set: function(target, prop, val) {
        if (!DOMApplicationRegistry.webapps[aId]) {
          return;
        }
        DOMApplicationRegistry.webapps[aId][prop] = val;
        return;
      },
    };
  },

  _fireEvent: function(aMessage) {
    let msg = aMessage.data;
    debug("_fireEvent " + JSON.stringify(msg));
    if (!this.DOMApps || !msg.manifestURL || !msg.eventType) {
      return;
    }

    let DOMApps = this.DOMApps[msg.manifestURL];
    if (!DOMApps) {
      return;
    }

    // The parent might ask childs to trigger more than one event in one
    // shot, so in order to avoid needless IPC we allow an array for the
    // 'eventType' IPC message field.
    if (!Array.isArray(msg.eventType)) {
      msg.eventType = [msg.eventType];
    }

    DOMApps.forEach((DOMApp) => {
      let domApp = DOMApp.get();
      if (!domApp) {
        return;
      }
      msg.eventType.forEach((aEventType) => {
        if ('on' + aEventType in domApp) {
          domApp._fireEvent(aEventType);
        }
      });

      if (msg.requestID) {
        aMessage.data.result = msg.manifestURL;
        domApp._fireRequestResult(aMessage);
      }
    });
  },

  _updateState: function(aMessage) {
    if (!this.DOMApps || !aMessage.id) {
      return;
    }

    let app = this.webapps[aMessage.id];
    if (!app) {
      return;
    }

    if (aMessage.app) {
      for (let prop in aMessage.app) {
        app[prop] = aMessage.app[prop];
      }
    }

    if ("error" in aMessage) {
      app.downloadError = aMessage.error;
    }

    if (aMessage.manifest) {
      app.manifest = aMessage.manifest;
      // Evict the wrapped manifest cache for all the affected DOM objects.
      let DOMApps = this.DOMApps[app.manifestURL];
      if (!DOMApps) {
        return;
      }
      DOMApps.forEach((DOMApp) => {
        let domApp = DOMApp.get();
        if (!domApp) {
          return;
        }
        WrappedManifestCache.evict(app.manifestURL, domApp.innerWindowID);
      });
    }
  },

  /**
   * nsIAppsService API
   */
  getAppByManifestURL: function getAppByManifestURL(aManifestURL) {
    debug("getAppByManifestURL " + aManifestURL);
    return AppsUtils.getAppByManifestURL(this.webapps, aManifestURL);
  },

  getAppLocalIdByManifestURL: function getAppLocalIdByManifestURL(aManifestURL) {
    debug("getAppLocalIdByManifestURL " + aManifestURL);
    return AppsUtils.getAppLocalIdByManifestURL(this.webapps, aManifestURL);
  },

  getCSPByLocalId: function(aLocalId) {
    debug("getCSPByLocalId:" + aLocalId);
    return AppsUtils.getCSPByLocalId(this.webapps, aLocalId);
  },

  getAppLocalIdByStoreId: function(aStoreId) {
    debug("getAppLocalIdByStoreId:" + aStoreId);
    return AppsUtils.getAppLocalIdByStoreId(this.webapps, aStoreId);
  },

  getAppByLocalId: function getAppByLocalId(aLocalId) {
    debug("getAppByLocalId " + aLocalId + " - ready: " + this.ready);
    let app = this.localIdIndex[aLocalId];
    if (!app) {
      debug("Ouch, No app!");
      return null;
    }

    return new mozIApplication(app);
  },

  getManifestURLByLocalId: function getManifestURLByLocalId(aLocalId) {
    debug("getManifestURLByLocalId " + aLocalId);
    return AppsUtils.getManifestURLByLocalId(this.webapps, aLocalId);
  },

  getCoreAppsBasePath: function getCoreAppsBasePath() {
    debug("getCoreAppsBasePath() not yet supported on child!");
    return null;
  },

  getWebAppsBasePath: function getWebAppsBasePath() {
    debug("getWebAppsBasePath() not yet supported on child!");
    return null;
  },

  getAppInfo: function getAppInfo(aAppId) {
    return AppsUtils.getAppInfo(this.webapps, aAppId);
  }
}

DOMApplicationRegistry.init();
