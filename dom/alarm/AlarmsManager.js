/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

const ALARMSMANAGER_CONTRACTID = "@mozilla.org/alarmsManager;1";
const ALARMSMANAGER_CID        = Components.ID("{fea1e884-9b05-11e1-9b64-87a7016c3860}");
const nsIDOMMozAlarmsManager   = Ci.nsIDOMMozAlarmsManager;
const nsIClassInfo             = Ci.nsIClassInfo;

function AlarmsManager()
{
}

AlarmsManager.prototype = {

  __proto__: DOMRequestIpcHelper.prototype,

  classID : ALARMSMANAGER_CID,

  QueryInterface : XPCOMUtils.generateQI([nsIDOMMozAlarmsManager, Ci.nsIDOMGlobalPropertyInitializer]),

  classInfo : XPCOMUtils.generateCI({ classID: ALARMSMANAGER_CID,
                                      contractID: ALARMSMANAGER_CONTRACTID,
                                      classDescription: "AlarmsManager",
                                      interfaces: [nsIDOMMozAlarmsManager],
                                      flags: nsIClassInfo.DOM_OBJECT }),

  add: function add(aDate, aRespectTimezone, aData) {
    return null;
  },

  remove: function remove(aId) {
    return;
  },

  getAll: function getAll() {
    return null;
  },

  // nsIDOMGlobalPropertyInitializer implementation
  init: function init(aWindow) {
    // Set navigator.mozAlarms to null.
    if (!Services.prefs.getBoolPref("dom.mozAlarms.enabled"))
      return null;

    let principal = aWindow.document.nodePrincipal;
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

    let perm = principal == secMan.getSystemPrincipal() ? 
      Ci.nsIPermissionManager.ALLOW_ACTION : Services.perms.testExactPermission(principal.URI, "alarms");

    // Only pages with perm set can use the alarms.
    this.hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;

    if (!this.hasPrivileges)
      return null;

    // Add the valid messages to be listened.
    this.initHelper(aWindow, []);
  },

  // Called from DOMRequestIpcHelper.
  uninit: function uninit() {
  },
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([AlarmsManager])
