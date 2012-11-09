/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// a place for miscellaneous social tests


const pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);


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
  testPrivateBrowsing: function(next) {
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          ok(true, "got sidebar message");
          port.close();
          togglePrivateBrowsing(function () {
            ok(!Social.enabled, "Social shuts down during private browsing");
            togglePrivateBrowsing(function () {
              ok(Social.enabled, "Social enabled after private browsing");
              next();
            });
          });
          break;
      }
    };
  },

  testPrivateBrowsingSocialDisabled: function(next) {
    // test PB from the perspective of entering PB without social enabled
    // we expect social to be enabled at the start of the test, we need
    // to disable it before testing PB transitions.
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          ok(true, "got sidebar message");
          port.close();
          Social.enabled = false;
          break;
      }
    }

    // wait for disable, then do some pb toggling. We expect social to remain
    // disabled through these tests
    Services.obs.addObserver(function observer(aSubject, aTopic) {
      Services.obs.removeObserver(observer, aTopic);
      ok(!Social.enabled, "Social is not enabled");
      togglePrivateBrowsing(function () {
        ok(!Social.enabled, "Social not available during private browsing");
        togglePrivateBrowsing(function () {
          ok(!Social.enabled, "Social is not enabled after private browsing");
          // social will be reenabled on start of next social test
          next();
        });
      });
    }, "social:pref-changed", false);
  }
}

function togglePrivateBrowsing(aCallback) {
  Services.obs.addObserver(function observe(subject, topic, data) {
    Services.obs.removeObserver(observe, topic);
    executeSoon(aCallback);
  }, "private-browsing-transition-complete", false);

  pb.privateBrowsingEnabled = !pb.privateBrowsingEnabled;
}
