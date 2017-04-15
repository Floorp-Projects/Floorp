/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var gWorkerAndCallback = {
  _worker: null,
  _callback: null,

  _ensureStarted: function() {
    if (!this._worker) {
      throw new Error("Not yet started!");
    }
  },

  start: function() {
    if (!this._worker) {
      var worker = new Worker("chrome://worker/content/worker.js");
      worker.onerror = function(event) {
        Cu.reportError(event.message);
        event.preventDefault();
      };

      this._worker = worker;
    }
  },

  stop: function() {
    if (this._worker) {
      try {
        this.terminate();
      }
      catch(e) {
        Cu.reportError(e);
      }
      this._worker = null;
    }
  },

  set callback(val) {
    this._ensureStarted();
    if (val) {
      var callback = val.QueryInterface(Ci.nsIWorkerTestCallback);
      if (this.callback != callback) {
        this._worker.onmessage = function(event) {
          callback.onmessage(event.data);
        };
        this._worker.onerror = function(event) {
          callback.onerror(event.message);
          event.preventDefault();
        };
        this._callback = callback;
      }
    }
    else {
      this._worker.onmessage = null;
      this._worker.onerror = null;
      this._callback = null;
    }
  },

  get callback() {
    return this._callback;
  },

  postMessage: function(data) {
    this._ensureStarted();
    this._worker.postMessage(data);
  },

  terminate: function() {
    this._ensureStarted();
    this._worker.terminate();
    this.callback = null;
  }
};

function WorkerTest() {
}
WorkerTest.prototype = {
  observe: function(subject, topic, data) {
    switch(topic) {
      case "profile-after-change":
        gWorkerAndCallback.start();
        Services.obs.addObserver(this, "profile-before-change");
        break;
      case "profile-before-change":
        gWorkerAndCallback.stop();
        break;
      default:
        Cu.reportError("Unknown topic: " + topic);
    }
  },

  set callback(val) {
    gWorkerAndCallback.callback = val;
  },

  get callback() {
    return gWorkerAndCallback.callback;
  },

  postMessage: function(message) {
    gWorkerAndCallback.postMessage(message);
  },

  terminate: function() {
    gWorkerAndCallback.terminate();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsIWorkerTest]),
  classID: Components.ID("{3b52b935-551f-4606-ba4c-decc18b67bfd}")
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WorkerTest]);
