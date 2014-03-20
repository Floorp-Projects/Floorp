/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* static functions */
const DEBUG = false;

function debug(aStr) {
  if (DEBUG)
    dump("AlarmsManager: " + aStr + "\n");
}

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
  debug("Constructor");
}

AlarmsManager.prototype = {

  __proto__: DOMRequestIpcHelper.prototype,

  classID : ALARMSMANAGER_CID,

  QueryInterface : XPCOMUtils.generateQI([nsIDOMMozAlarmsManager,
                                          Ci.nsIDOMGlobalPropertyInitializer,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIObserver]),

  classInfo : XPCOMUtils.generateCI({ classID: ALARMSMANAGER_CID,
                                      contractID: ALARMSMANAGER_CONTRACTID,
                                      classDescription: "AlarmsManager",
                                      interfaces: [nsIDOMMozAlarmsManager],
                                      flags: nsIClassInfo.DOM_OBJECT }),

  add: function add(aDate, aRespectTimezone, aData) {
    debug("add()");

    if (!this._manifestURL) {
      debug("Cannot add alarms for non-installed apps.");
      throw Components.results.NS_ERROR_FAILURE;
    }

    if (!aDate) {
      throw Components.results.NS_ERROR_INVALID_ARG;
    }

    let isIgnoreTimezone = true;
    switch (aRespectTimezone) {
      case "honorTimezone":
        isIgnoreTimezone = false;
        break;

      case "ignoreTimezone":
        isIgnoreTimezone = true;
        break;

      default:
        throw Components.results.NS_ERROR_INVALID_ARG;
        break;
    }

    let request = this.createRequest();
    this._cpmm.sendAsyncMessage(
      "AlarmsManager:Add",
      { requestId: this.getRequestId(request),
        date: aDate,
        ignoreTimezone: isIgnoreTimezone,
        data: aData,
        pageURL: this._pageURL,
        manifestURL: this._manifestURL }
    );
    return request;
  },

  remove: function remove(aId) {
    debug("remove()");

    this._cpmm.sendAsyncMessage(
      "AlarmsManager:Remove",
      { id: aId, manifestURL: this._manifestURL }
    );
  },

  getAll: function getAll() {
    debug("getAll()");

    let request = this.createRequest();
    this._cpmm.sendAsyncMessage(
      "AlarmsManager:GetAll",
      { requestId: this.getRequestId(request), manifestURL: this._manifestURL }
    );
    return request;
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("receiveMessage(): " + aMessage.name);

    let json = aMessage.json;
    let request = this.getRequest(json.requestId);

    if (!request) {
      debug("No request stored! " + json.requestId);
      return;
    }

    switch (aMessage.name) {
      case "AlarmsManager:Add:Return:OK":
        Services.DOMRequest.fireSuccess(request, json.id);
        break;

      case "AlarmsManager:GetAll:Return:OK":
        // We don't need to expose everything to the web content.
        let alarms = [];
        json.alarms.forEach(function trimAlarmInfo(aAlarm) {
          let alarm = { "id":              aAlarm.id,
                        "date":            aAlarm.date,
                        "respectTimezone": aAlarm.ignoreTimezone ?
                                             "ignoreTimezone" : "honorTimezone",
                        "data":            aAlarm.data };
          alarms.push(alarm);
        });
        Services.DOMRequest.fireSuccess(request,
                                        Cu.cloneInto(alarms, this._window));
        break;

      case "AlarmsManager:Add:Return:KO":
        Services.DOMRequest.fireError(request, json.errorMsg);
        break;

      case "AlarmsManager:GetAll:Return:KO":
        Services.DOMRequest.fireError(request, json.errorMsg);
        break;

      default:
        debug("Wrong message: " + aMessage.name);
        break;
    }
    this.removeRequest(json.requestId);
   },

  // nsIDOMGlobalPropertyInitializer implementation
  init: function init(aWindow) {
    debug("init()");

    // Set navigator.mozAlarms to null.
    if (!Services.prefs.getBoolPref("dom.mozAlarms.enabled")) {
      return null;
    }

    // Only pages with perm set can use the alarms.
    let principal = aWindow.document.nodePrincipal;
    let perm =
      Services.perms.testExactPermissionFromPrincipal(principal, "alarms");
    if (perm != Ci.nsIPermissionManager.ALLOW_ACTION) {
      return null;
    }

    // SystemPrincipal documents do not have any origin.
    // Reject them for now.
    if (!principal.URI) {
      return null;
    }

    this._cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                   .getService(Ci.nsISyncMessageSender);

    // Add the valid messages to be listened.
    this.initDOMRequestHelper(aWindow, ["AlarmsManager:Add:Return:OK",
                                        "AlarmsManager:Add:Return:KO",
                                        "AlarmsManager:GetAll:Return:OK",
                                        "AlarmsManager:GetAll:Return:KO"]);

    // Get the manifest URL if this is an installed app
    let appsService = Cc["@mozilla.org/AppsService;1"]
                        .getService(Ci.nsIAppsService);
    this._pageURL = principal.URI.spec;
    this._manifestURL = appsService.getManifestURLByLocalId(principal.appId);
    this._window = aWindow;
  },

  // Called from DOMRequestIpcHelper.
  uninit: function uninit() {
    debug("uninit()");
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AlarmsManager])
