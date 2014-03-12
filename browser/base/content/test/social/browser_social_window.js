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
  Social.providers = [];
  // *sob* - listeners keep getting added...
  SocialService._providerListeners.clear();
}

let createdWindows = [];

function openWindowAndWaitForInit(parentWin, callback) {
  // this notification tells us SocialUI.init() has been run...
  let topic = "browser-delayed-startup-finished";
  let w = parentWin.OpenBrowserWindow();
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
                    info("window closed, " + createdWindows.length + " windows left");
                    closeOneWindow(cb);
                    }, "window did not close");
  w.close();
}

function postTestCleanup(cb) {
  closeOneWindow(cb);
}

let manifest = { // normal provider
  name: "provider 1",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};
let manifest2 = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
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
    openWindowAndWaitForInit(window, function(w1) {
      checkSocialUI(w1);
      // Now social is (re-)initialized, open a secondary window and check that.
      openWindowAndWaitForInit(window, function(w2) {
        checkSocialUI(w2);
        checkSocialUI(w1);
        cbnext();
      });
    });
  },

  // Check when providers are enabled and social is turned on at startup.
  testEnabledStartup: function(cbnext) {
    setManifestPref("social.manifest.test", manifest);
    ok(!SocialSidebar.opened, "sidebar is closed initially");
    SocialService.addProvider(manifest, function() {
      SocialService.addProvider(manifest2, function (provider) {
        SocialSidebar.show();
        waitForCondition(function() SocialSidebar.opened,
                     function() {
          ok(SocialSidebar.opened, "first window sidebar is open");
          openWindowAndWaitForInit(window, function(w1) {
            ok(w1.SocialSidebar.opened, "new window sidebar is open");
            ok(SocialService.hasEnabledProviders, "providers are enabled");
            checkSocialUI(w1);
            // now init is complete, open a second window
            openWindowAndWaitForInit(window, function(w2) {
              ok(w1.SocialSidebar.opened, "w1 sidebar is open");
              ok(w2.SocialSidebar.opened, "w2 sidebar is open");
              checkSocialUI(w2);
              checkSocialUI(w1);

              // disable social and re-check
              SocialService.removeProvider(manifest.origin, function() {
                SocialService.removeProvider(manifest2.origin, function() {
                  ok(!Social.enabled, "social is disabled");
                  is(Social.providers.length, 0, "no providers");
                  ok(!w1.SocialSidebar.opened, "w1 sidebar is closed");
                  ok(!w2.SocialSidebar.opened, "w2 sidebar is closed");
                  checkSocialUI(w2);
                  checkSocialUI(w1);
                  Services.prefs.clearUserPref("social.manifest.test");
                  cbnext();
                });
              });
            });
          });
        }, "sidebar did not open");
      }, cbnext);
    }, cbnext);
  },

  // Check per window sidebar functionality, including migration from using
  // prefs to using session state, and state inheritance of windows (new windows
  // inherit state from the opener).
  testPerWindowSidebar: function(cbnext) {
    function finishCheck() {
      // disable social and re-check
      SocialService.removeProvider(manifest.origin, function() {
        SocialService.removeProvider(manifest2.origin, function() {
          ok(!Social.enabled, "social is disabled");
          is(Social.providers.length, 0, "no providers");
          Services.prefs.clearUserPref("social.manifest.test");
          cbnext();
        });
      });
    }

    setManifestPref("social.manifest.test", manifest);
    ok(!SocialSidebar.opened, "sidebar is closed initially");
    SocialService.addProvider(manifest, function() {
      SocialService.addProvider(manifest2, function (provider) {
        // test the migration of the social.sidebar.open pref. We'll set a user
        // level pref to indicate it was open (along with the old
        // social.provider.current pref), then we'll open a window. During the
        // restoreState of the window, those prefs should be migrated, and the
        // sidebar should be opened.  Both prefs are then removed.
        Services.prefs.setCharPref("social.provider.current", "https://example.com");
        Services.prefs.setBoolPref("social.sidebar.open", true);

        openWindowAndWaitForInit(window, function(w1) {
          ok(w1.SocialSidebar.opened, "new window sidebar is open");
          ok(SocialService.hasEnabledProviders, "providers are enabled");
          ok(!Services.prefs.prefHasUserValue("social.provider.current"), "social.provider.current pref removed");
          ok(!Services.prefs.prefHasUserValue("social.sidebar.open"), "social.sidebar.open pref removed");
          checkSocialUI(w1);
          // now init is complete, open a second window, it's state should be the same as the opener
          openWindowAndWaitForInit(w1, function(w2) {
            ok(w1.SocialSidebar.opened, "w1 sidebar is open");
            ok(w2.SocialSidebar.opened, "w2 sidebar is open");
            checkSocialUI(w2);
            checkSocialUI(w1);

            // change the sidebar in w2
            w2.SocialSidebar.show(manifest2.origin);
            let sbrowser1 = w1.document.getElementById("social-sidebar-browser");
            is(manifest.origin, sbrowser1.getAttribute("origin"), "w1 sidebar origin matches");
            let sbrowser2 = w2.document.getElementById("social-sidebar-browser");
            is(manifest2.origin, sbrowser2.getAttribute("origin"), "w2 sidebar origin matches");

            // hide sidebar, w1 sidebar should still be open
            w2.SocialSidebar.hide();
            ok(w1.SocialSidebar.opened, "w1 sidebar is opened");
            ok(!w2.SocialSidebar.opened, "w2 sidebar is closed");
            ok(sbrowser2.parentNode.hidden, "w2 sidebar is hidden");

            // open a 3rd window from w2, it should inherit the state of w2
            openWindowAndWaitForInit(w2, function(w3) {
              // since the sidebar is not open, we need to ensure the provider
              // is selected to test we inherited the provider from the opener
              w3.SocialSidebar.ensureProvider();
              is(w3.SocialSidebar.provider, w2.SocialSidebar.provider, "w3 has same provider as w2");
              ok(!w3.SocialSidebar.opened, "w2 sidebar is closed");

              // open a 4th window from w1, it should inherit the state of w1
              openWindowAndWaitForInit(w1, function(w4) {
                is(w4.SocialSidebar.provider, w1.SocialSidebar.provider, "w4 has same provider as w1");
                ok(w4.SocialSidebar.opened, "w4 sidebar is opened");

                finishCheck();
              });
            });

          });
        });
      }, cbnext);
    }, cbnext);
  }
}
