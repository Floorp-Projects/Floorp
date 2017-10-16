/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["indexedDB"]);

function GlobalObjectsComponent() {
  this.wrappedJSObject = this;
}

GlobalObjectsComponent.prototype =
{
  classID: Components.ID("{949ebf50-e0da-44b9-8335-cbfd4febfdcc}"),

  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsISupports]),

  runTest() {
    const name = "Splendid Test";

    let ok = this.ok;
    let finishTest = this.finishTest;

    let keyRange = IDBKeyRange.only(42);
    ok(keyRange, "Got keyRange");

    let request = indexedDB.open(name, 1);
    request.onerror = function(event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      finishTest();
    };
    request.onsuccess = function(event) {
      let db = event.target.result;
      ok(db, "Got database");
      finishTest();
    };
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([GlobalObjectsComponent]);
