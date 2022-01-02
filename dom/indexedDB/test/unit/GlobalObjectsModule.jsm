/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.importGlobalProperties(["indexedDB"]);

var EXPORTED_SYMBOLS = ["GlobalObjectsModule"];

function GlobalObjectsModule() {}

GlobalObjectsModule.prototype = {
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
  },
};
