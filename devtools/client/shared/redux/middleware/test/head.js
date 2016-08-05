/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var flags = require("devtools/shared/flags");
var promise = require("promise");
var defer = require("devtools/shared/defer");

flags.testing = true;

function waitUntilState(store, predicate) {
  let deferred = defer();
  let unsubscribe = store.subscribe(check);

  function check() {
    if (predicate(store.getState())) {
      unsubscribe();
      deferred.resolve();
    }
  }

  // Fire the check immediately incase the action has already occurred
  check();

  return deferred.promise;
}
