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

    gContentAPI.getConfiguration("sync", callback);
  },

  function test_checkSyncSetup_enabled(done) {
    function callback(result) {
      is(result.setup, true, "Sync should be setup");
      done();
    }

    Services.prefs.setCharPref("services.sync.username", "uitour@tests.mozilla.org");
    gContentAPI.getConfiguration("sync", callback);
  },

  // The showFirefoxAccounts API is sync related, so we test that here too...
  function test_firefoxAccounts(done) {
    // This test will load about:accounts, and we don't want that to hit the
    // network.
    Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri",
                               "https://example.com/");

    loadUITourTestPage(function(contentWindow) {
      let tabBrowser = gBrowser.selectedBrowser;
      // This command will replace the current tab - so add a load event
      // handler which will fire when that happens.
      tabBrowser.addEventListener("load", function onload(evt) {
        tabBrowser.removeEventListener("load", onload, true);

        is(tabBrowser.contentDocument.location.href,
           "about:accounts?action=signup&entrypoint=uitour",
           "about:accounts should have replaced the tab");

        // the iframe in about:accounts will still be loading, so we stop
        // that before resetting the pref.
        tabBrowser.contentDocument.location.href = "about:blank";
        Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
        done();
      }, true);

      gContentAPI.showFirefoxAccounts();
    });
  },
];
