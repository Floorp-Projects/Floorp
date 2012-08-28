/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

// This module exposes a subset of the functionnalities of the parent DOM
// Registry to content processes, to be be used from the AppsService component.

let EXPORTED_SYMBOLS = ["DOMApplicationRegistry"];

Cu.import("resource://gre/modules/AppsUtils.jsm");

function debug(s) {
  //dump("-*- AppsServiceChild.jsm: " + s + "\n");
}

let DOMApplicationRegistry = {
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
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("Received " + aMessage.name + " message.");
    let msg = aMessage.json;
    switch (aMessage.name) {
      case "Webapps:AddApp":
        this.webapps[msg.id] = msg.app;
        break;
      case "Webapps:RemoveApp":
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

  getAppByLocalId: function getAppByLocalId(aLocalId) {
    debug("getAppByLocalId " + aLocalId);
    return AppsUtils.getAppByLocalId(this.webapps, aLocalId);
  },

  getManifestURLByLocalId: function getManifestURLByLocalId(aLocalId) {
    debug("getManifestURLByLocalId " + aLocalId);
    return AppsUtils.getManifestURLByLocalId(this.webapps, aLocalId);
  }
}

DOMApplicationRegistry.init();
