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

function AlarmsManager() {
  debug("Constructor");
}

AlarmsManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  contractID : "@mozilla.org/alarmsManager;1",

  classID : Components.ID("{fea1e884-9b05-11e1-9b64-87a7016c3860}"),

  QueryInterface : XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                          Ci.nsISupportsWeakReference,
                                          Ci.nsIObserver]),

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

    let sandbox = new Cu.Sandbox(Cu.getWebIDLCallerPrincipal());
    sandbox.data = aData;
    let data = Cu.evalInSandbox("JSON.stringify(data)", sandbox);
    let request = this.createRequest();
    this._cpmm.sendAsyncMessage("AlarmsManager:Add",
                                { requestId: this.getRequestId(request),
                                  date: aDate,
                                  ignoreTimezone: isIgnoreTimezone,
                                  data: data,
                                  pageURL: this._pageURL,
                                  manifestURL: this._manifestURL });
    return request;
  },

  remove: function remove(aId) {
    debug("remove()");

    this._cpmm.sendAsyncMessage("AlarmsManager:Remove",
                                { id: aId, manifestURL: this._manifestURL });
  },

  getAll: function getAll() {
    debug("getAll()");

    let request = this.createRequest();
    this._cpmm.sendAsyncMessage("AlarmsManager:GetAll",
                                { requestId: this.getRequestId(request),
                                  manifestURL: this._manifestURL });
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
          let sandbox = new Cu.Sandbox(this._principal);
          sandbox.data = aAlarm.data;
          let data = Cu.evalInSandbox("JSON.parse(data)", sandbox);
          let alarm = { "id": aAlarm.id,
                        "date": aAlarm.date,
                        "respectTimezone": aAlarm.ignoreTimezone ?
                                             "ignoreTimezone" : "honorTimezone",
                        "data": data };
          alarms.push(alarm);
        }.bind(this));

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
    this._window = aWindow;
    this._principal = this._window.document.nodePrincipal;
    this._pageURL = this._principal.URI.spec;
    this._manifestURL =
      appsService.getManifestURLByLocalId(this._principal.appId);
  },

  // Called from DOMRequestIpcHelper.
  uninit: function uninit() {
    debug("uninit()");
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AlarmsManager])
