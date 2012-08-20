/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "http://example.com",
    sidebarURL: "http://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "http://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testStatusIcons: function(next) {
    let iconsReady = false;
    let gotSidebarMessage = false;

    function checkNext() {
      if (iconsReady && gotSidebarMessage)
        triggerIconPanel();
    }

    function triggerIconPanel() {
      let statusIcons = document.getElementById("social-status-iconbox");
      ok(!statusIcons.firstChild.hidden, "status icon is visible");
      // Click the button to trigger its contentPanel
      let panel = document.getElementById("social-notification-panel");
      EventUtils.synthesizeMouseAtCenter(statusIcons.firstChild, {});
    }

    let port = Social.provider.port;
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-panel-message":
          ok(true, "got panel message");
          break;
        case "got-social-panel-visibility":
          if (e.data.result == "shown") {
            ok(true, "panel shown");
            let panel = document.getElementById("social-notification-panel");
            panel.hidePopup();
          } else if (e.data.result == "hidden") {
            ok(true, "panel hidden");
            next();
          }
        case "got-sidebar-message":
          // The sidebar message will always come first, since it loads by default
          ok(true, "got sidebar message");
          gotSidebarMessage = true;
          checkNext();
          break;
      }
    }
    port.postMessage({topic: "test-init"});

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
  },

  testServiceWindow: function(next) {
    // our test provider was initialized in the test above, we just
    // initiate our specific test now.
    let port = Social.provider.port;
    ok(port, "provider has a port");
    port.postMessage({topic: "test-service-window"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-service-window-message":
          // The sidebar message will always come first, since it loads by default
          ok(true, "got service window message");
          port.postMessage({topic: "test-close-service-window"});
          break;
        case "got-service-window-closed-message":
          ok(true, "got service window closed message");
          next();
          break;
      }
    }
  },

  testServiceWindowTwice: function(next) {
    let port = Social.provider.port;
    port.postMessage({topic: "test-service-window-twice"});
    Social.provider.port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-service-window-twice-result":
          is(e.data.result, "ok", "only one window should open when name is reused");
          break;
        case "got-service-window-message":
          ok(true, "got service window message");
          port.postMessage({topic: "test-close-service-window"});
          break;
        case "got-service-window-closed-message":
          ok(true, "got service window closed message");
          next();
          break;
      }
    }
  }
}
