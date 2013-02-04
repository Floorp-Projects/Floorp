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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const APPS_SERVICE_CID = Components.ID("{05072afa-92fe-45bf-ae22-39b69c117058}");

function AppsService()
{
  debug("AppsService Constructor");
  let inParent = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                   .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  debug("inParent: " + inParent);
  Cu.import(inParent ? "resource://gre/modules/Webapps.jsm" :
                       "resource://gre/modules/AppsServiceChild.jsm");
}

AppsService.prototype = {

  getCSPByLocalId: function getCSPByLocalId(localId) {
    debug("GetCSPByLocalId( " + localId + " )");
    return DOMApplicationRegistry.getCSPByLocalId(localId);
  },

  getAppByManifestURL: function getAppByManifestURL(aManifestURL) {
    debug("GetAppByManifestURL( " + aManifestURL + " )");
    return DOMApplicationRegistry.getAppByManifestURL(aManifestURL);
  },

  getAppLocalIdByManifestURL: function getAppLocalIdByManifestURL(aManifestURL) {
    debug("getAppLocalIdByManifestURL( " + aManifestURL + " )");
    return DOMApplicationRegistry.getAppLocalIdByManifestURL(aManifestURL);
  },

  getAppByLocalId: function getAppByLocalId(aLocalId) {
    debug("getAppByLocalId( " + aLocalId + " )");
    return DOMApplicationRegistry.getAppByLocalId(aLocalId);
  },

  getManifestURLByLocalId: function getManifestURLByLocalId(aLocalId) {
    debug("getManifestURLByLocalId( " + aLocalId + " )");
    return DOMApplicationRegistry.getManifestURLByLocalId(aLocalId);
  },

  getAppFromObserverMessage: function getAppFromObserverMessage(aMessage) {
    debug("getAppFromObserverMessage( " + aMessage + " )");
    return DOMApplicationRegistry.getAppFromObserverMessage(aMessage);
  },

  getCoreAppsBasePath: function getCoreAppsBasePath() {
    debug("getCoreAppsBasePath()");
    return DOMApplicationRegistry.getCoreAppsBasePath();
  },

  getWebAppsBasePath: function getWebAppsBasePath() {
    debug("getWebAppsBasePath()");
    return DOMApplicationRegistry.getWebAppsBasePath();
  },

  getAppInfo: function getAppInfo(aAppId) {
    debug("getAppInfo()");
    return DOMApplicationRegistry.getAppInfo(aAppId);
  },

  classID : APPS_SERVICE_CID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIAppsService])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AppsService])
