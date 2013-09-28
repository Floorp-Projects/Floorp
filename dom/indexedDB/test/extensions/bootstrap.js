/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function testForExpectedSymbols(stage, data) {
  const expectedSymbols = [ "IDBKeyRange", "indexedDB" ];
  for each (var symbol in expectedSymbols) {
    Services.prefs.setBoolPref("indexeddbtest.bootstrap." + stage + "." +
                               symbol, symbol in this);
  }
}

function GlobalObjectsComponent() {
  this.wrappedJSObject = this;
}

GlobalObjectsComponent.prototype =
{
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  runTest: function() {
    const name = "Splendid Test";

    let ok = this.ok;
    let finishTest = this.finishTest;

    let keyRange = IDBKeyRange.only(42);
    ok(keyRange, "Got keyRange");

    let request = indexedDB.open(name, 1);
    request.onerror = function(event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      finishTest();
    }
    request.onsuccess = function(event) {
      let db = event.target.result;
      ok(db, "Got database");
      finishTest();
    }
  }
};

var gFactory = {
  register: function() {
    var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

    var classID = Components.ID("{d6f85dcb-537d-447e-b783-75d4b405622d}");
    var description = "IndexedDBTest";
    var contractID = "@mozilla.org/dom/indexeddb/GlobalObjectsComponent;1";
    var factory = XPCOMUtils._getFactory(GlobalObjectsComponent);

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
}

function shutdown(data, reason) {
  testForExpectedSymbols("shutdown");
  gFactory.unregister();
}

function uninstall(data, reason) {
  testForExpectedSymbols("uninstall");
}
