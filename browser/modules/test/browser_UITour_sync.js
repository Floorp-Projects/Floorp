/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("services.sync.username");
  });
  UITourTest();
}

let tests = [
  function test_checkSyncSetup_disabled(done) {
    function callback(result) {
      is(result.setup, false, "Sync shouldn't be setup by default");
      done();
    }

    gContentAPI.getSyncConfiguration(callback);
  },

  function test_checkSyncSetup_enabled(done) {
    function callback(result) {
      is(result.setup, true, "Sync should be setup");
      done();
    }

    Services.prefs.setCharPref("services.sync.username", "uitour@tests.mozilla.org");
    gContentAPI.getSyncConfiguration(callback);
  },
];
