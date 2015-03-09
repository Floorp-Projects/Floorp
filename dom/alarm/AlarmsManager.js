/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* static functions */
const DEBUG = false;
const REQUEST_CPU_LOCK_TIMEOUT = 10 * 1000; // 10 seconds.

function debug(aStr) {
  if (DEBUG)
    dump("AlarmsManager: " + aStr + "\n");
}

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gPowerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

function AlarmsManager() {
  debug("Constructor");

  // A <requestId, {cpuLock, timer}> map.
  this._cpuLockDict = new Map();
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

    let data = aData;
    if (aData) {
      // Run JSON.stringify() in the sand box with the principal of the calling
      // web page to ensure no cross-origin object is involved. A "Permission
      // Denied" error will be thrown in case of privilege violation.
      let sandbox = new Cu.Sandbox(Cu.getWebIDLCallerPrincipal());
      sandbox.data = aData;
      data = JSON.parse(Cu.evalInSandbox("JSON.stringify(data)", sandbox));
    }
    let request = this.createRequest();
    let requestId = this.getRequestId(request);
    this._lockCpuForRequest(requestId);
    this._cpmm.sendAsyncMessage("AlarmsManager:Add",
                                { requestId: requestId,
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
        this._unlockCpuForRequest(json.requestId);
        Services.DOMRequest.fireSuccess(request, json.id);
        break;

      case "AlarmsManager:GetAll:Return:OK":
        // We don't need to expose everything to the web content.
        let alarms = [];
        json.alarms.forEach(function trimAlarmInfo(aAlarm) {
          let alarm = { "id": aAlarm.id,
                        "date": aAlarm.date,
                        "respectTimezone": aAlarm.ignoreTimezone ?
                                             "ignoreTimezone" : "honorTimezone",
                        "data": aAlarm.data };
          alarms.push(alarm);
        });

        Services.DOMRequest.fireSuccess(request,
                                        Cu.cloneInto(alarms, this._window));
        break;

      case "AlarmsManager:Add:Return:KO":
        this._unlockCpuForRequest(json.requestId);
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
    let principal = aWindow.document.nodePrincipal;
    this._pageURL = principal.URI.spec;
    this._manifestURL = appsService.getManifestURLByLocalId(principal.appId);
    this._window = aWindow;
  },

  // Called from DOMRequestIpcHelper.
  uninit: function uninit() {
    debug("uninit()");
  },

  _lockCpuForRequest: function (aRequestId) {
    if (this._cpuLockDict.has(aRequestId)) {
      debug('Cpu wakelock for request ' + aRequestId + ' has been acquired. ' +
            'You may call this function repeatedly or requestId is collision.');
      return;
    }

    // Acquire a lock for given request and save for lookup lately.
    debug('Acquire cpu lock for request ' + aRequestId);
    let cpuLockInfo = {
      cpuLock: gPowerManagerService.newWakeLock("cpu"),
      timer: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer)
    };
    this._cpuLockDict.set(aRequestId, cpuLockInfo);

    // Start a timer to prevent from non-responding request.
    cpuLockInfo.timer.initWithCallback(() => {
      debug('Request timeout! Release the cpu lock');
      this._unlockCpuForRequest(aRequestId);
    }, REQUEST_CPU_LOCK_TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _unlockCpuForRequest: function(aRequestId) {
    let cpuLockInfo = this._cpuLockDict.get(aRequestId);
    if (!cpuLockInfo) {
      debug('The cpu lock for requestId ' + aRequestId + ' is either invalid ' +
            'or has been released.');
      return;
    }

    // Release the cpu lock and cancel the timer.
    debug('Release the cpu lock for ' + aRequestId);
    cpuLockInfo.cpuLock.unlock();
    cpuLockInfo.timer.cancel();
    this._cpuLockDict.delete(aRequestId);
  },

}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AlarmsManager])
