/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testOpenCloseFlyout: function(next) {
    let panel = document.getElementById("social-flyout-panel");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          port.postMessage({topic: "test-flyout-open"});
          break;
        case "got-flyout-visibility":
          if (e.data.result == "hidden") {
            ok(true, "flyout visibility is 'hidden'");
            is(panel.state, "closed", "panel really is closed");
            port.close();
            next();
          } else if (e.data.result == "shown") {
            ok(true, "flyout visibility is 'shown");
            port.postMessage({topic: "test-flyout-close"});
          }
          break;
        case "got-flyout-message":
          ok(e.data.result == "ok", "got flyout message");
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  },

  testResizeFlyout: function(next) {
    let panel = document.getElementById("social-flyout-panel");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-flyout-open"});
          break;
        case "got-flyout-visibility":
          // The width of the flyout should be 250px
          let iframe = panel.firstChild;
          let cs = iframe.contentWindow.getComputedStyle(iframe.contentDocument.body);
          is(cs.width, "250px", "should be 250px wide");
          iframe.contentDocument.addEventListener("SocialTest-DoneMakeWider", function _doneHandler() {
            iframe.contentDocument.removeEventListener("SocialTest-DoneMakeWider", _doneHandler, false);
            cs = iframe.contentWindow.getComputedStyle(iframe.contentDocument.body);
            is(cs.width, "500px", "should now be 500px wide");
            panel.hidePopup();
            port.close();
            next();
          }, false);
          SocialFlyout.dispatchPanelEvent("socialTest-MakeWider");
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  },

  testCloseSelf: function(next) {
    let panel = document.getElementById("social-flyout-panel");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-flyout-open"});
          break;
        case "got-flyout-visibility":
          let iframe = panel.firstChild;
          iframe.contentDocument.addEventListener("SocialTest-DoneCloseSelf", function _doneHandler() {
            iframe.contentDocument.removeEventListener("SocialTest-DoneCloseSelf", _doneHandler, false);
            is(panel.state, "closed", "flyout should have closed itself");
            next();
          }, false);
          is(panel.state, "open", "flyout should be open");
          port.close(); // so we don't get the -visibility message as it hides...
          SocialFlyout.dispatchPanelEvent("socialTest-CloseSelf");
          break;
      }
    }
    port.postMessage({topic: "test-init"});
  }
}
