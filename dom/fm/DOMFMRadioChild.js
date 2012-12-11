/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"
let DEBUG = 0;
if (DEBUG)
  debug = function (s) { dump("-*- DOMFMRadioChild: " + s + "\n"); };
else
  debug = function (s) { };

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

const DOMFMMANAGER_CONTRACTID = "@mozilla.org/domfmradio;1";
const DOMFMMANAGER_CID = Components.ID("{901f8a83-03a6-4be9-bb8f-35387d3849da}");

XPCOMUtils.defineLazyGetter(Services, "DOMRequest", function() {
  return Cc["@mozilla.org/dom/dom-request-service;1"]
           .getService(Ci.nsIDOMRequestService);
});

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

function DOMFMRadioChild() { }

DOMFMRadioChild.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classID: DOMFMMANAGER_CID,
  classInfo: XPCOMUtils.generateCI({
               classID: DOMFMMANAGER_CID,
               contractID: DOMFMMANAGER_CONTRACTID,
               classDescription: "DOMFMRadio",
               interfaces: [Ci.nsIDOMFMRadio],
               flags: Ci.nsIClassInfo.DOM_OBJECT
             }),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMFMRadio,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"]
                   .getService(Ci.nsIScriptSecurityManager);

    let perm = Services.perms.testExactPermissionFromPrincipal(aWindow.document.nodePrincipal, "fmradio");
    this._hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;

    if (!this._hasPrivileges) {
      Cu.reportError("NO FMRADIO PERMISSION FOR: " + aWindow.document.nodePrincipal.origin + "\n");
      return null;
    }

    const messages = ["DOMFMRadio:enable:Return:OK",
                      "DOMFMRadio:enable:Return:NO",
                      "DOMFMRadio:disable:Return:OK",
                      "DOMFMRadio:disable:Return:NO",
                      "DOMFMRadio:setFrequency:Return:OK",
                      "DOMFMRadio:setFrequency:Return:NO",
                      "DOMFMRadio:seekUp:Return:OK",
                      "DOMFMRadio:seekUp:Return:NO",
                      "DOMFMRadio:seekDown:Return:OK",
                      "DOMFMRadio:seekDown:Return:NO",
                      "DOMFMRadio:cancelSeek:Return:OK",
                      "DOMFMRadio:cancelSeek:Return:NO",
                      "DOMFMRadio:frequencyChange",
                      "DOMFMRadio:powerStateChange",
                      "DOMFMRadio:antennaChange"];
    this.initHelper(aWindow, messages);

    let els = Cc["@mozilla.org/eventlistenerservice;1"]
                .getService(Ci.nsIEventListenerService);

    els.addSystemEventListener(aWindow, "visibilitychange",
                               this._updateVisibility.bind(this),
                               /* useCapture = */ true);

    this._visibility = aWindow.document.visibilityState;
    // Unlike the |enabled| getter, this is true if *this* DOM window
    // has successfully enabled the FM radio more recently than
    // disabling it.
    this._haveEnabledRadio = false;
  },

  // Called from DOMRequestIpcHelper
  uninit: function() {
    this._onFrequencyChange = null;
    this._onAntennaChange = null;
    this._onDisabled = null;
    this._onEnabled = null;
  },

  _createEvent: function(name) {
    return new this._window.Event(name);
  },

  _sendMessageForRequest: function(name, data, request) {
    let id = this.getRequestId(request);
    cpmm.sendAsyncMessage(name, {
      data: data,
      rid: id,
      mid: this._id
    });
  },

  _fireFrequencyChangeEvent: function() {
    let e = this._createEvent("frequencychange");
    if (this._onFrequencyChange) {
      this._onFrequencyChange.handleEvent(e);
    }
    this.dispatchEvent(e);
  },

  _firePowerStateChangeEvent: function() {
    let _enabled = this.enabled;
    debug("Current power state: " + _enabled);
    if (_enabled) {
      let e = this._createEvent("enabled");
      if (this._onEnabled) {
        this._onEnabled.handleEvent(e);
      }
      this.dispatchEvent(e);
    } else {
      let e = this._createEvent("disabled");
      if (this._onDisabled) {
        this._onDisabled.handleEvent(e);
      }
      this.dispatchEvent(e);
    }
  },

  _fireAntennaAvailableChangeEvent: function() {
    let e = this._createEvent("antennaavailablechange");
    if (this._onAntennaChange) {
      this._onAntennaChange.handleEvent(e);
    }
    this.dispatchEvent(e);
  },

  _updateVisibility: function(evt) {
    this._visibility = evt.target.visibilityState;
    // Only notify visibility state when we "own" the radio stream.
    if (this._haveEnabledRadio) {
      this._notifyVisibility();
    }
  },

  _notifyVisibility: function() {
    cpmm.sendAsyncMessage("DOMFMRadio:updateVisibility", this._visibility);
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    if (msg.mid && msg.mid != this._id) {
      return;
    }

    let request;
    switch (aMessage.name) {
      case "DOMFMRadio:enable:Return:OK":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireSuccess(request, null);
        break;
      case "DOMFMRadio:enable:Return:NO":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireError(request, "Failed to turn on the FM radio");
        break;
      case "DOMFMRadio:disable:Return:OK":
        this._haveEnabledRadio = false;
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireSuccess(request, null);
        break;
      case "DOMFMRadio:disable:Return:NO":
        // If disabling the radio failed, but the hardware is still
        // on, this DOM window is still responsible for the continued
        // playback.
        this._haveEnabledRadio = this.enabled;
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireError(request,
                                      "Failed to turn off the FM radio");
        break;
      case "DOMFMRadio:setFrequency:Return:OK":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireSuccess(request, null);
        break;
      case "DOMFMRadio:setFrequency:Return:NO":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireError(request,
                                      "Failed to set the FM radio frequency");
        break;
      case "DOMFMRadio:seekUp:Return:OK":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireSuccess(request, null);
        break;
      case "DOMFMRadio:seekUp:Return:NO":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireError(request, "FM radio seek-up failed");
        break;
      case "DOMFMRadio:seekDown:Return:OK":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireSuccess(request, null);
        break;
      case "DOMFMRadio:seekDown:Return:NO":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireError(request, "FM radio seek-down failed");
        break;
      case "DOMFMRadio:cancelSeek:Return:OK":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireSuccess(request, null);
        break;
      case "DOMFMRadio:cancelSeek:Return:NO":
        request = this.takeRequest(msg.rid);
        if (!request) {
          return;
        }
        Services.DOMRequest.fireError(request, "Failed to cancel seek");
        break;
      case "DOMFMRadio:powerStateChange":
        this._firePowerStateChangeEvent();
        break;
      case "DOMFMRadio:frequencyChange":
        this._fireFrequencyChangeEvent();
        break;
      case "DOMFMRadio:antennaChange":
        this._fireAntennaAvailableChangeEvent();
        break;
    }
  },

  _call: function(name, arg) {
    var request = this.createRequest();
    this._sendMessageForRequest("DOMFMRadio:" + name, arg, request);
    return request;
  },

  // nsIDOMFMRadio
  get enabled() {
    return cpmm.sendSyncMessage("DOMFMRadio:getPowerState")[0];
  },

  get antennaAvailable() {
    return cpmm.sendSyncMessage("DOMFMRadio:getAntennaState")[0];
  },

  get frequency() {
    return cpmm.sendSyncMessage("DOMFMRadio:getFrequency")[0];
  },

  get frequencyUpperBound() {
    let range = cpmm.sendSyncMessage("DOMFMRadio:getCurrentBand")[0];
    return range.upper;
  },

  get frequencyLowerBound() {
    let range = cpmm.sendSyncMessage("DOMFMRadio:getCurrentBand")[0];
    return range.lower;
  },

  get channelWidth() {
    let range = cpmm.sendSyncMessage("DOMFMRadio:getCurrentBand")[0];
    return range.channelWidth;
  },

  set onantennaavailablechange(callback) {
    this._onAntennaChange = callback;
  },

  set onenabled(callback) {
    this._onEnabled = callback;
  },

  set ondisabled(callback) {
    this._onDisabled = callback;
  },

  set onfrequencychange(callback) {
    this._onFrequencyChange = callback;
  },

  disable: function nsIDOMFMRadio_disable() {
    return this._call("disable", null);
  },

  enable: function nsIDOMFMRadio_enable(frequency) {
    // FMRadio::Enable() needs the most recent visibility state
    // synchronously.
    this._haveEnabledRadio = true;
    this._notifyVisibility();
    return this._call("enable", frequency);
  },

  setFrequency: function nsIDOMFMRadio_setFreq(frequency) {
    return this._call("setFrequency", frequency);
  },

  seekDown: function nsIDOMFMRadio_seekDown() {
    return this._call("seekDown", null);
  },

  seekUp: function nsIDOMFMRadio_seekUp() {
    return this._call("seekUp", null);
  },

  cancelSeek: function nsIDOMFMRadio_cancelSeek() {
    return this._call("cancelSeek", null);
  },

  // These are fake implementations, will be replaced by using
  // nsJSDOMEventTargetHelper, see bug 731746
  addEventListener: function(type, listener, useCapture) {
    if (!this._eventListenersByType) {
      this._eventListenersByType = {};
    }

    if (!listener) {
      return;
    }

    var listeners = this._eventListenersByType[type];
    if (!listeners) {
      listeners = this._eventListenersByType[type] = [];
    }

    useCapture = !!useCapture;
    for (let i = 0, len = listeners.length; i < len; i++) {
      let l = listeners[i];
      if (l && l.listener === listener && l.useCapture === useCapture) {
        return;
      }
    }

    listeners.push({
      listener: listener,
      useCapture: useCapture
    });
  },

  removeEventListener: function(type, listener, useCapture) {
    if (!this._eventListenersByType) {
      return;
    }

    useCapture = !!useCapture;

    var listeners = this._eventListenersByType[type];
    if (listeners) {
      for (let i = 0, len = listeners.length; i < len; i++) {
        let l = listeners[i];
        if (l && l.listener === listener && l.useCapture === useCapture) {
          listeners.splice(i, 1);
        }
      }
    }
  },

  dispatchEvent: function(evt) {
    if (!this._eventListenersByType) {
      return;
    }

    let type = evt.type;
    var listeners = this._eventListenersByType[type];
    if (listeners) {
      for (let i = 0, len = listeners.length; i < len; i++) {
        let listener = listeners[i].listener;

        try {
          if (typeof listener == "function") {
            listener.call(this, evt);
          } else if (listener && listener.handleEvent &&
                     typeof listener.handleEvent == "function") {
            listener.handleEvent(evt);
          }
        } catch (e) {
          debug("Exception is caught: " + e);
        }
      }
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DOMFMRadioChild]);

