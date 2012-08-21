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
    iconURL: "chrome://branding/content/icon48.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testSidebarMessage: function(next) {
    let port = Social.provider.port;
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    Social.provider.port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          // The sidebar message will always come first, since it loads by default
          ok(true, "got sidebar message");
          next();
          break;
      }
    };
  },
  testIsVisible: function(next) {
    let port = Social.provider.port;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-isVisible-response":
          is(e.data.result, true, "Sidebar should be visible by default");
          Social.toggleSidebar();
          next();
      }
    };
    port.postMessage({topic: "test-isVisible"});
  },
  testIsNotVisible: function(next) {
    let port = Social.provider.port;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-isVisible-response":
          is(e.data.result, false, "Sidebar should be hidden");
          Services.prefs.clearUserPref("social.sidebar.open");
          next();
      }
    };
    port.postMessage({topic: "test-isVisible"});
  }
}
