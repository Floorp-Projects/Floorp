/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

// This module exposes a subset of the functionnalities of the parent DOM
// Registry to content processes, to be be used from the AppsService component.

this.EXPORTED_SYMBOLS = ["DOMApplicationRegistry"];

Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function debug(s) {
  //dump("-*- AppsServiceChild.jsm: " + s + "\n");
}

this.DOMApplicationRegistry = {
  init: function init() {
    debug("init");
    this.cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                  .getService(Ci.nsISyncMessageSender);

    ["Webapps:AddApp", "Webapps:RemoveApp"].forEach((function(aMsgName) {
      this.cpmm.addMessageListener(aMsgName, this);
    }).bind(this));

    // We need to prime the cache with the list of apps.
    // XXX shoud we do this async and block callers if it's not yet there?
    this.webapps = this.cpmm.sendSyncMessage("Webapps:GetList", { })[0];

    // We need a fast mapping from localId -> app, so we add an index.
    this.localIdIndex = { };
    for (let id in this.webapps) {
      let app = this.webapps[id];
      this.localIdIndex[app.localId] = app;
    }

    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function(aSubject, aTopic, aData) {
    // cpmm.addMessageListener causes the DOMApplicationRegistry object to live
    // forever if we don't clean up properly.
    this.webapps = null;
    ["Webapps:AddApp", "Webapps:RemoveApp"].forEach((function(aMsgName) {
      this.cpmm.removeMessageListener(aMsgName, this);
    }).bind(this));
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("Received " + aMessage.name + " message.");
    let msg = aMessage.json;
    switch (aMessage.name) {
      case "Webapps:AddApp":
        this.webapps[msg.id] = msg.app;
        this.localIdIndex[msg.app.localId] = msg.app;
        break;
      case "Webapps:RemoveApp":
        delete this.localIdIndex[this.webapps[msg.id].localId];
        delete this.webapps[msg.id];
        break;
    }
  },

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
    debug("getAppByLocalId " + aLocalId);
    let app = this.localIdIndex[aLocalId];
    if (!app) {
      debug("Ouch, No app!");
      return null;
    }

    return AppsUtils.cloneAsMozIApplication(app);
  },

  getManifestURLByLocalId: function getManifestURLByLocalId(aLocalId) {
    debug("getManifestURLByLocalId " + aLocalId);
    return AppsUtils.getManifestURLByLocalId(this.webapps, aLocalId);
  },

  getAppFromObserverMessage: function getAppFromObserverMessage(aMessage) {
    debug("getAppFromObserverMessage " + aMessage);
    return AppsUtils.getAppFromObserverMessage(this.webapps. aMessage);
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
