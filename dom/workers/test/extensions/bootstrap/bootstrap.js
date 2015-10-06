/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function testForExpectedSymbols(stage, data) {
  const expectedSymbols = [ "Worker", "ChromeWorker" ];
  for each (var symbol in expectedSymbols) {
    Services.prefs.setBoolPref("workertest.bootstrap." + stage + "." + symbol,
                               symbol in this);
  }
}

var gWorkerAndCallback = {
  _ensureStarted: function() {
    if (!this._worker) {
      throw new Error("Not yet started!");
    }
  },

  start: function(data) {
    if (!this._worker) {
      var file = data.installPath;
      var fileuri = file.isDirectory() ?
                    Services.io.newFileURI(file) :
                    Services.io.newURI('jar:' + file.path + '!/', null, null);
      var resourceName = encodeURIComponent(data.id);

      Services.io.getProtocolHandler("resource").
                  QueryInterface(Ci.nsIResProtocolHandler).
                  setSubstitution(resourceName, fileuri);

      this._worker = new Worker("resource://" + resourceName + "/worker.js");
      this._worker.onerror = function(event) {
        Cu.reportError(event.message);
        event.preventDefault();
      };
    }
  },

  stop: function() {
    if (this._worker) {
      this._worker.terminate();
      delete this._worker;
    }
  },

  set callback(val) {
    this._ensureStarted();
    var callback = val.QueryInterface(Ci.nsIObserver);
    if (this._callback != callback) {
      if (callback) {
        this._worker.onmessage = function(event) {
          callback.observe(this, event.type, event.data);
        };
        this._worker.onerror = function(event) {
          callback.observe(this, event.type, event.message);
          event.preventDefault();
        };
      }
      else {
        this._worker.onmessage = null;
        this._worker.onerror = null;
      }
      this._callback = callback;
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
    delete this._callback;
  }
};

function WorkerTestBootstrap() {
}
WorkerTestBootstrap.prototype = {
  observe: function(subject, topic, data) {

    gWorkerAndCallback.callback = subject;

    switch (topic) {
      case "postMessage":
        gWorkerAndCallback.postMessage(data);
        break;

      case "terminate":
        gWorkerAndCallback.terminate();
        break;

      default:
        throw new Error("Unknown worker command");
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
};

var gFactory = {
  register: function() {
    var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

    var classID = Components.ID("{36b5df0b-8dcf-4aa2-9c45-c51d871295f9}");
    var description = "WorkerTestBootstrap";
    var contractID = "@mozilla.org/test/workertestbootstrap;1";
    var factory = XPCOMUtils._getFactory(WorkerTestBootstrap);

    registrar.registerFactory(classID, description, contractID, factory);

    this.unregister = function() {
      registrar.unregisterFactory(classID, factory);
      delete this.unregister;
    };
  }
};

function install(data, reason) {
  testForExpectedSymbols("install");
}

function startup(data, reason) {
  testForExpectedSymbols("startup");
  gFactory.register();
  gWorkerAndCallback.start(data);
}

function shutdown(data, reason) {
  testForExpectedSymbols("shutdown");
  gWorkerAndCallback.stop();
  gFactory.unregister();
}

function uninstall(data, reason) {
  testForExpectedSymbols("uninstall");
}
