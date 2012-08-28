/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(s) {
  //dump("-*- AppsService: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const APPS_SERVICE_CID = Components.ID("{05072afa-92fe-45bf-ae22-39b69c117058}");

function AppsService()
{
  debug("AppsService Constructor");
  this.inParent = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                  .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  debug("inParent: " + this.inParent);
  if (this.inParent) {
    Cu.import("resource://gre/modules/Webapps.jsm");
  } else {
    this.cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                .getService(Ci.nsISyncMessageSender);
  }
}

AppsService.prototype = {
  getAppByManifestURL: function getAppByManifestURL(aManifestURL) {
    debug("GetAppByManifestURL( " + aManifestURL + " )");
    if (this.inParent) {
      return DOMApplicationRegistry.getAppByManifestURL(aManifestURL);
    } else {
      return this.cpmm.sendSyncMessage("WebApps:GetAppByManifestURL",
                                       { url: aManifestURL })[0];
    }
  },

  getAppLocalIdByManifestURL: function getAppLocalIdByManifestURL(aManifestURL) {
    debug("getAppLocalIdByManifestURL( " + aManifestURL + " )");
    if (this.inParent) {
      return DOMApplicationRegistry.getAppLocalIdByManifestURL(aManifestURL);
    } else {
      let res = this.cpmm.sendSyncMessage("WebApps:GetAppLocalIdByManifestURL",
                                          { url: aManifestURL })[0];
      return res.id;
    }
  },

  getAppByLocalId: function getAppByLocalId(aLocalId) {
    debug("getAppByLocalId( " + aLocalId + " )");
    if (this.inParent) {
      return DOMApplicationRegistry.getAppByLocalId(aLocalId);
    } else {
      return this.cpmm.sendSyncMessage("WebApps:GetAppByLocalId",
                                       { id: aLocalId })[0];
    }
  },

  getManifestURLByLocalId: function getManifestURLByLocalId(aLocalId) {
    debug("getManifestURLByLocalId( " + aLocalId + " )");
    if (this.inParent) {
      return DOMApplicationRegistry.getManifestURLByLocalId(aLocalId);
    } else {
      return this.cpmm.sendSyncMessage("WebApps:GetManifestURLByLocalId",
                                       { id: aLocalId })[0];
    }
  },

  classID : APPS_SERVICE_CID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIAppsService])
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([AppsService])
