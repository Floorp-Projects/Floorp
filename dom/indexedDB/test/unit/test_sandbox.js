/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function exerciseInterface() {
  function DB(name, store) {
    this.name = name;
    this.store = store;
    this._db = this._create();
  }

  DB.prototype = {
    _create() {
      var op = indexedDB.open(this.name);
      op.onupgradeneeded = e => {
        var db = e.target.result;
        db.createObjectStore(this.store);
      };
      return new Promise(resolve => {
        op.onsuccess = e => resolve(e.target.result);
      });
    },

    _result(tx, op) {
      return new Promise((resolve, reject) => {
        op.onsuccess = e => resolve(e.target.result);
        op.onerror = () => reject(op.error);
        tx.onabort = () => reject(tx.error);
      });
    },

    get(k) {
      return this._db.then(db => {
        var tx = db.transaction(this.store, "readonly");
        var store = tx.objectStore(this.store);
        return this._result(tx, store.get(k));
      });
    },

    add(k, v) {
      return this._db.then(db => {
        var tx = db.transaction(this.store, "readwrite");
        var store = tx.objectStore(this.store);
        return this._result(tx, store.add(v, k));
      });
    }
  };

  var db = new DB("data", "base");
  return db.add("x", [ 10, {} ])
    .then(_ => db.get("x"))
    .then(x => {
      equal(x.length, 2);
      equal(x[0], 10);
      equal(typeof x[1], "object");
      equal(Object.keys(x[1]).length, 0);
    });
}

function run_test() {
  do_get_profile();

  let Cu = Components.utils;
  let sb = new Cu.Sandbox("https://www.example.com",
                          { wantGlobalProperties: ["indexedDB"] });

  sb.equal = equal;
  var innerPromise = new Promise((resolve, reject) => {
    sb.test_done = resolve;
    sb.test_error = reject;
  });
  Cu.evalInSandbox("(" + exerciseInterface.toSource() + ")()" +
                   ".then(test_done, test_error);", sb);

  Cu.importGlobalProperties(["indexedDB"]);
  do_test_pending();
  Promise.all([innerPromise, exerciseInterface()])
    .then(do_test_finished);
}
