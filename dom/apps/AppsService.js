/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(s) {
  //dump("-*- AppsService.js: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const APPS_SERVICE_CID = Components.ID("{05072afa-92fe-45bf-ae22-39b69c117058}");

function AppsService()
{
  debug("AppsService Constructor");
  this.inParent = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                    .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  debug("inParent: " + this.inParent);
  if (!this.inParent) {
    Cu.import("resource://gre/modules/AppsServiceChild.jsm");
  }
}

AppsService.prototype = {

  isInvalidId: function(localId) {
    return (localId == Ci.nsIScriptSecurityManager.NO_APP_ID ||
            localId == Ci.nsIScriptSecurityManager.UNKNOWN_APP_ID);
  },

  getManifestFor: function getManifestFor(aManifestURL) {
    debug("getManifestFor(" + aManifestURL + ")");
    if (this.inParent) {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    } else {
      return Promise.reject(
        new Error("Calling getManifestFor() from child is not supported"));
    }
  },

  getAppLocalIdByManifestURL: function getAppLocalIdByManifestURL(aManifestURL) {
    debug("getAppLocalIdByManifestURL( " + aManifestURL + " )");
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getAppLocalIdByStoreId: function getAppLocalIdByStoreId(aStoreId) {
    debug("getAppLocalIdByStoreId( " + aStoreId + " )");
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getManifestURLByLocalId: function getManifestURLByLocalId(aLocalId) {
    debug("getManifestURLByLocalId( " + aLocalId + " )");
    if (this.isInvalidId(aLocalId)) {
      return null;
    }
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getCoreAppsBasePath: function getCoreAppsBasePath() {
    debug("getCoreAppsBasePath()");
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getWebAppsBasePath: function getWebAppsBasePath() {
    debug("getWebAppsBasePath()");
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  areAnyAppsInstalled: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getAppInfo: function getAppInfo(aAppId) {
    debug("getAppInfo()");
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  getScopeByLocalId: function(aLocalId) {
    debug("getScopeByLocalId( " + aLocalId + " )");
    if (this.isInvalidId(aLocalId)) {
      return null;
    }
    // TODO : implement properly!
    // We just return null for now to not break PushService.jsm
    return null;
  },

  classID : APPS_SERVICE_CID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIAppsService])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AppsService])
