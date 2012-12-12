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
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testSidebarMessage: function(next) {
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          // The sidebar message will always come first, since it loads by default
          ok(true, "got sidebar message");
          port.close();
          next();
          break;
      }
    };
  },
  testIsVisible: function(next) {
    let port = Social.provider.getWorkerPort();
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-isVisible-response":
          is(e.data.result, true, "Sidebar should be visible by default");
          Social.toggleSidebar();
          port.close();
          next();
      }
    };
    port.postMessage({topic: "test-isVisible"});
  },
  testIsNotVisible: function(next) {
    let port = Social.provider.getWorkerPort();
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-isVisible-response":
          is(e.data.result, false, "Sidebar should be hidden");
          Services.prefs.clearUserPref("social.sidebar.open");
          port.close();
          next();
      }
    };
    port.postMessage({topic: "test-isVisible"});
  }
}
