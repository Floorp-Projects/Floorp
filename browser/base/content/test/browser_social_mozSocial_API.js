/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

function test() {
  // XXX Bug 775779
  if (Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2).isDebugBuild) {
    ok(true, "can't run social sidebar test in debug builds because they falsely report leaks");
    return;
  }

  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "http://example.com",
    sidebarURL: "http://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "http://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  runSocialTestWithProvider(manifest, doTest);
}

function doTest() {
  let iconsReady = false;
  let gotSidebarMessage = false;

  function checkNext() {
    if (iconsReady && gotSidebarMessage)
      triggerIconPanel();
  }

  function triggerIconPanel() {
    let statusIcons = document.getElementById("social-status-iconbox");
    ok(!statusIcons.firstChild.collapsed, "status icon is visible");
    // Click the button to trigger its contentPanel
    let panel = document.getElementById("social-notification-panel");
    EventUtils.synthesizeMouseAtCenter(statusIcons.firstChild, {});
  }

  let port = Social.provider.port;
  ok(port, "provider has a port");
  port.postMessage({topic: "test-init"});
  Social.provider.port.onmessage = function (e) {
    let topic = e.data.topic;
    switch (topic) {
      case "got-panel-message":
        ok(true, "got panel message");
        // Wait for the panel to close before ending the test
        let panel = document.getElementById("social-notification-panel");
        panel.addEventListener("popuphidden", function hiddenListener() {
          panel.removeEventListener("popuphidden", hiddenListener);
          SocialService.removeProvider(Social.provider.origin, finish);
        });
        panel.hidePopup();
        break;
      case "got-sidebar-message":
        // The sidebar message will always come first, since it loads by default
        ok(true, "got sidebar message");
        info(topic);
        gotSidebarMessage = true;
        checkNext();
        break;
    }
  }

  // Our worker sets up ambient notification at the same time as it responds to
  // the workerAPI initialization. If it's already initialized, we can
  // immediately check the icons, otherwise wait for initialization by
  // observing the topic sent out by the social service.
  if (Social.provider.workerAPI.initialized) {
    iconsReady = true;
    checkNext();
  } else {
    Services.obs.addObserver(function obs() {
      Services.obs.removeObserver(obs, "social:ambient-notification-changed");
      // Let the other observers (like the one that updates the UI) run before
      // checking the icons.
      executeSoon(function () {
        iconsReady = true;
        checkNext();
      });
    }, "social:ambient-notification-changed", false);
  }
}
