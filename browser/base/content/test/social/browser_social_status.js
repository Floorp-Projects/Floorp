/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let manifest = { // builtin provider
  name: "provider example.com",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
};
let manifest2 = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  statusURL: "https://test1.example.com/browser/browser/base/content/test/social/social_panel.html",
  iconURL: "https://test1.example.com/browser/browser/base/content/test/moz.png",
  version: 1
};
let manifest3 = { // used for testing install
  name: "provider test2",
  origin: "https://test2.example.com",
  sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://test2.example.com/browser/browser/base/content/test/moz.png",
  version: 1
};


function openWindowAndWaitForInit(callback) {
  let topic = "browser-delayed-startup-finished";
  let w = OpenBrowserWindow();
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, topic);
    executeSoon(() => callback(w));
  }, topic, false);
}

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("social.allowMultipleWorkers", true);
  let toolbar = document.getElementById("nav-bar");
  let currentsetAtStart = toolbar.currentSet;
  info("tb0 "+currentsetAtStart);
  runSocialTestWithProvider(manifest, function () {
    runSocialTests(tests, undefined, undefined, function () {
      Services.prefs.clearUserPref("social.remote-install.enabled");
      // just in case the tests failed, clear these here as well
      Services.prefs.clearUserPref("social.allowMultipleWorkers");
      Services.prefs.clearUserPref("social.whitelist");
      
      // This post-test test ensures that a new window maintains the same
      // toolbar button set as when we started. That means our insert/removal of
      // persistent id's is working correctly
      is(currentsetAtStart, toolbar.currentSet, "toolbar currentset unchanged");
      openWindowAndWaitForInit(function(w1) {
        checkSocialUI(w1);
        // Sometimes the new window adds other buttons to currentSet that are
        // outside the scope of what we're checking. So we verify that all
        // buttons from startup are in currentSet for a new window, and that the
        // provider buttons are properly removed. (e.g on try, window-controls
        // was not present in currentsetAtStart, but present on the second
        // window)
        let tb1 = w1.document.getElementById("nav-bar");
        info("tb0 "+toolbar.currentSet);
        info("tb1 "+tb1.currentSet);
        let startupSet = Set(toolbar.currentSet.split(','));
        let newSet = Set(tb1.currentSet.split(','));
        let intersect = Set([x for (x of startupSet) if (newSet.has(x))]);
        info("intersect "+intersect);
        let difference = Set([x for (x of newSet) if (!startupSet.has(x))]);
        info("difference "+difference);
        is(startupSet.size, intersect.size, "new window toolbar same as old");
        // verify that our provider buttons are not in difference
        let id = SocialStatus._toolbarHelper.idFromOrgin(manifest2.origin);
        ok(!difference.has(id), "status button not persisted at end");
        w1.close();
        finish();
      });
    });
  });
}

var tests = {
  testNoButtonOnInstall: function(next) {
    // we expect the addon install dialog to appear, we need to accept the
    // install from the dialog.
    info("Waiting for install dialog");
    let panel = document.getElementById("servicesInstall-notification");
    PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
      PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
      info("servicesInstall-notification panel opened");
      panel.button.click();
    })

    let id = "social-status-button-" + manifest3.origin;
    let toolbar = document.getElementById("nav-bar");
    let currentset = toolbar.getAttribute("currentset").split(',');
    ok(currentset.indexOf(id) < 0, "button is not part of currentset at start");

    let activationURL = manifest3.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      Social.installProvider(doc, manifest3, function(addonManifest) {
        // enable the provider so we know the button would have appeared
        SocialService.addBuiltinProvider(manifest3.origin, function(provider) {
          ok(provider, "provider is installed");
          currentset = toolbar.getAttribute("currentset").split(',');
          ok(currentset.indexOf(id) < 0, "button was not added to currentset");
          Social.uninstallProvider(manifest3.origin, function() {
            gBrowser.removeTab(tab);
            next();
          });
        });
      });
    });
  },
  testButtonOnInstall: function(next) {
    // we expect the addon install dialog to appear, we need to accept the
    // install from the dialog.
    info("Waiting for install dialog");
    let panel = document.getElementById("servicesInstall-notification");
    PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
      PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
      info("servicesInstall-notification panel opened");
      panel.button.click();
    })

    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      Social.installProvider(doc, manifest2, function(addonManifest) {
          // at this point, we should have a button id in the currentset for our provider
          let id = "social-status-button-" + manifest2.origin;
          let toolbar = document.getElementById("nav-bar");

          waitForCondition(function() {
                             let currentset = toolbar.getAttribute("currentset").split(',');
                             return currentset.indexOf(id) >= 0;
                           },
                           function() {
                             // no longer need the tab
                             gBrowser.removeTab(tab);
                             next();
                           }, "status button added to currentset");
      });
    });
  },
  testButtonOnEnable: function(next) {
    // enable the provider now
    SocialService.addBuiltinProvider(manifest2.origin, function(provider) {
      ok(provider, "provider is installed");
      let id = "social-status-button-" + manifest2.origin;
      waitForCondition(function() { return document.getElementById(id) },
                       next, "button exists after enabling social");
    });
  },
  testStatusPanel: function(next) {
    let icon = {
      name: "testIcon",
      iconURL: "chrome://browser/skin/Info.png",
      counter: 1
    };
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    let id = "social-status-button-" + provider.origin;
    let btn = document.getElementById(id)
    ok(btn, "got a status button");
    let port = provider.getWorkerPort();

    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          ok(true, "test-init-done received");
          ok(provider.profile.userName, "profile was set by test worker");
          btn.click();
          break;
        case "got-social-panel-visibility":
          ok(true, "got the panel message " + e.data.result);
          if (e.data.result == "shown") {
            let panel = document.getElementById("social-notification-panel");
            panel.hidePopup();
          } else {
            port.postMessage({topic: "test-ambient-notification", data: icon});
            port.close();
            waitForCondition(function() { return btn.getAttribute("badge"); },
                       function() {
                         is(btn.style.listStyleImage, "url(\"" + icon.iconURL + "\")", "notification icon updated");
                         next();
                       }, "button updated by notification");
          }
          break;
      }
    };
    port.postMessage({topic: "test-init"});
  },
  testButtonOnDisable: function(next) {
    // enable the provider now
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider, "provider is installed");
    SocialService.removeProvider(manifest2.origin, function() {
      let id = "social-status-button-" + manifest2.origin;
      waitForCondition(function() { return !document.getElementById(id) },
                       next, "button does not exist after disabling the provider");
    });
  },
  testButtonOnUninstall: function(next) {
    Social.uninstallProvider(manifest2.origin, function() {
      // test that the button is no longer persisted
      let id = "social-status-button-" + manifest2.origin;
      let toolbar = document.getElementById("nav-bar");
      let currentset = toolbar.getAttribute("currentset").split(',');
      is(currentset.indexOf(id), -1, "button no longer in currentset");
      next();
    });
  }
}
