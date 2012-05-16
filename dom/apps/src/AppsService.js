/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

function debug(s) {
  //dump("-*- AppsService: " + s + "\n");
}

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");

const APPS_SERVICE_CONTRACTID = "@mozilla.org/AppsService;1";
const APPS_SERVICE_CID        = Components.ID("{05072afa-92fe-45bf-ae22-39b69c117058}");

function AppsService()
{
  debug("AppsService Constructor");
}

AppsService.prototype = {
  getAppByManifestURL: function getAppByManifestURL(aManifestURL) {
    debug("GetAppByManifestURL( " + aManifestURL + " )");
    return DOMApplicationRegistry.getAppByManifestURL(aManifestURL)
  },

  classID : APPS_SERVICE_CID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIAppsService]),

  classInfo : XPCOMUtils.generateCI({classID: APPS_SERVICE_CID,
                                     contractID: APPS_SERVICE_CONTRACTID,
                                     classDescription: "AppsService",
                                     interfaces: [Ci.nsIAppsService],
                                     flags: Ci.nsIClassInfo.DOM_OBJECT})
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([AppsService])
