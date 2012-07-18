/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

function test() {
  waitForExplicitFinish();

  let oldProvider;
  function saveOldProviderAndStartTestWith(provider) {
    oldProvider = Social.provider;
    registerCleanupFunction(function () {
      Social.provider = oldProvider;
    });
    Social.provider = provider;

    // Now that we've set the UI's provider, enable the social functionality
    Services.prefs.setBoolPref("social.enabled", true);
    registerCleanupFunction(function () {
      Services.prefs.clearUserPref("social.enabled");
    });

    doTest();
  }

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example1.com",
    sidebarURL: "https://example1.com/sidebar.html",
    workerURL: "https://example1.com/worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  SocialService.addProvider(manifest, function(provider) {
    // If the UI is already active, run the test immediately, otherwise wait
    // for initialization.
    if (Social.provider) {
      saveOldProviderAndStartTestWith(provider);
    } else {
      Services.obs.addObserver(function obs() {
        Services.obs.removeObserver(obs, "test-social-ui-ready");
        saveOldProviderAndStartTestWith(provider);
      }, "test-social-ui-ready", false);
    }
  });
}

function doTest() {
  ok(SocialSidebar.canShow, "social sidebar should be able to be shown");
  ok(SocialSidebar.enabled, "social sidebar should be on by default");

  let command = document.getElementById("Social:ToggleSidebar");
  let sidebar = document.getElementById("social-sidebar-box");

  // Check the the sidebar is initially visible, and loaded
  ok(!command.hidden, "sidebar toggle command should be visible");
  is(command.getAttribute("checked"), "true", "sidebar toggle command should be checked");
  ok(!sidebar.hidden, "sidebar itself should be visible");
  ok(Services.prefs.getBoolPref("social.sidebar.open"), "sidebar open pref should be true");
  is(sidebar.firstChild.getAttribute('src'), "https://example1.com/sidebar.html", "sidebar url should be set");

  // Now toggle it!
  info("Toggling sidebar");
  Social.toggleSidebar();
  is(command.getAttribute("checked"), "false", "sidebar toggle command should not be checked");
  ok(sidebar.hidden, "sidebar itself should not be visible");
  ok(!Services.prefs.getBoolPref("social.sidebar.open"), "sidebar open pref should be false");
  is(sidebar.firstChild.getAttribute('src'), "about:blank", "sidebar url should not be set");

  // Remove the test provider
  SocialService.removeProvider(Social.provider.origin, finish);
}

// XXX test sidebar in popup
