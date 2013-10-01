// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test the top-level window UI for social.

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

// This function should "reset" Social such that the next time Social.init()
// is called (eg, when a new window is opened), it re-performs all
// initialization.
function resetSocial() {
  Social.initialized = false;
  Social._provider = null;
  Social.providers = [];
  // *sob* - listeners keep getting added...
  SocialService._providerListeners.clear();
}

let createdWindows = [];

function openWindowAndWaitForInit(callback) {
  // this notification tells us SocialUI.init() has been run...
  let topic = "browser-delayed-startup-finished";
  let w = OpenBrowserWindow();
  createdWindows.push(w);
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, topic);
    info(topic + " observer was notified - continuing test");
    executeSoon(() => callback(w));
  }, topic, false);
}

function closeOneWindow(cb) {
  let w = createdWindows.pop();
  if (!w) {
    cb();
    return;
  }
  waitForCondition(function() w.closed,
                   function() {
                    closeOneWindow(cb);
                    }, "window did not close");
  w.close();
}

function postTestCleanup(cb) {
  closeOneWindow(function() {
    Services.prefs.clearUserPref("social.enabled");
    cb();
  });
}

let manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};

function test() {
  waitForExplicitFinish();
  runSocialTests(tests, undefined, postTestCleanup);
}

let tests = {
  // check when social is totally disabled at startup (ie, no providers enabled)
  testInactiveStartup: function(cbnext) {
    is(Social.providers.length, 0, "needs zero providers to start this test.");
    ok(!SocialService.hasEnabledProviders, "no providers are enabled");
    resetSocial();
    openWindowAndWaitForInit(function(w1) {
      checkSocialUI(w1);
      // Now social is (re-)initialized, open a secondary window and check that.
      openWindowAndWaitForInit(function(w2) {
        checkSocialUI(w2);
        checkSocialUI(w1);
        cbnext();
      });
    });
  },

  // Check when providers are enabled and social is turned on at startup.
  testEnabledStartup: function(cbnext) {
    runSocialTestWithProvider(manifest, function (finishcb) {
      resetSocial();
      openWindowAndWaitForInit(function(w1) {
        ok(Social.enabled, "social is enabled");
        ok(SocialService.hasEnabledProviders, "providers are enabled");
        checkSocialUI(w1);
        // now init is complete, open a second window
        openWindowAndWaitForInit(function(w2) {
          checkSocialUI(w2);
          checkSocialUI(w1);
          // disable social and re-check
          Services.prefs.setBoolPref("social.enabled", false);
          executeSoon(function() { // let all the UI observers run...
            ok(!Social.enabled, "social is disabled");
            checkSocialUI(w2);
            checkSocialUI(w1);
            finishcb();
          });
        });
      });
    }, cbnext);
  },

  // Check when providers are enabled but social is turned off at startup.
  testDisabledStartup: function(cbnext) {
    setManifestPref("social.manifest.test", manifest);
    SocialService.addProvider(manifest, function (provider) {
      Services.prefs.setBoolPref("social.enabled", false);
      resetSocial();
      ok(SocialService.hasEnabledProviders, "providers are enabled");
      openWindowAndWaitForInit(function(w1) {
        ok(!Social.enabled, "social is disabled");
        checkSocialUI(w1);
        // now init is complete, open a second window
        openWindowAndWaitForInit(function(w2) {
          checkSocialUI(w2);
          checkSocialUI(w1);
          // enable social and re-check
          Services.prefs.setBoolPref("social.enabled", true);
          executeSoon(function() { // let all the UI observers run...
            ok(Social.enabled, "social is enabled");
            checkSocialUI(w2);
            checkSocialUI(w1);
            SocialService.removeProvider(manifest.origin, function() {
              Services.prefs.clearUserPref("social.manifest.test");
              cbnext();
            });
          });
        });
      });
    }, cbnext);
  },

  // Check when the last provider is disabled.
  testRemoveProvider: function(cbnext) {
    setManifestPref("social.manifest.test", manifest);
    SocialService.addProvider(manifest, function (provider) {
      openWindowAndWaitForInit(function(w1) {
        checkSocialUI(w1);
        // now init is complete, open a second window
        openWindowAndWaitForInit(function(w2) {
          checkSocialUI(w2);
          // disable the current provider.
          SocialService.removeProvider(manifest.origin, function() {
            ok(!Social.enabled, "social is disabled");
            is(Social.providers.length, 0, "no providers");
            checkSocialUI(w2);
            checkSocialUI(w1);
            Services.prefs.clearUserPref("social.manifest.test");
            cbnext();
          });
        });
      });
    }, cbnext);
  },
}
