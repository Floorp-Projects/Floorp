/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testStatusIcons: function(next) {
    let icon = {
      name: "testIcon",
      iconURL: "chrome://browser/skin/Info.png",
      contentPanel: "https://example.com/browser/browser/base/content/test/social/social_panel.html",
      counter: 1
    };

    let iconsReady = false;
    let gotSidebarMessage = false;

    function checkNext() {
      if (iconsReady && gotSidebarMessage)
        triggerIconPanel();
    }

    function triggerIconPanel() {
      let pButton = document.getElementById("social-provider-button");
      waitForCondition(function() {
        // wait for a new button to be inserted inbetween the provider and mark
        // button
        return !!pButton.nextSibling;
      }, function() {
        // Click the button to trigger its contentPanel
        let statusIcon = pButton.nextSibling;
        EventUtils.synthesizeMouseAtCenter(statusIcon, {});
      }, "Status icon didn't become non-hidden");
    }

    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          iconsReady = true;
          checkNext();
          break;
        case "got-panel-message":
          ok(true, "got panel message");
          // Check the panel isn't in our history.
          gURLsNotRemembered.push(e.data.location);
          break;
        case "got-social-panel-visibility":
          if (e.data.result == "shown") {
            ok(true, "panel shown");
            let panel = document.getElementById("social-notification-panel");
            panel.hidePopup();
          } else if (e.data.result == "hidden") {
            ok(true, "panel hidden");
            port.close();
            next();
          }
          break;
        case "got-sidebar-message":
          // The sidebar message will always come first, since it loads by default
          ok(true, "got sidebar message");
          gotSidebarMessage = true;
          // load a status panel
          port.postMessage({topic: "test-ambient-notification", data: icon});
          checkNext();
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  }
}
