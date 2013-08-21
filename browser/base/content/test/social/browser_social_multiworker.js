/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("social.allowMultipleWorkers", true);
  runSocialTestWithProvider(gProviders, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function() {
      Services.prefs.clearUserPref("social.allowMultipleWorkers");
      finishcb();
    });
  });
}

let gProviders = [
  {
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html?provider1",
    workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  },
  {
    name: "provider 2",
    origin: "https://test1.example.com",
    sidebarURL: "https://test1.example.com/browser/browser/base/content/test/social/social_sidebar.html?provider2",
    workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  }
];

var tests = {
  testWorkersAlive: function(next) {
    // verify we can get a message from all providers that are enabled
    let messageReceived = 0;
    function oneWorkerTest(provider) {
      let port = provider.getWorkerPort();
      port.onmessage = function (e) {
        let topic = e.data.topic;
        switch (topic) {
          case "test-init-done":
            ok(true, "got message from provider " + provider.name);
            port.close();
            messageReceived++;
            break;
        }
      };
      port.postMessage({topic: "test-init"});
    }

    for (let p of Social.providers) {
      oneWorkerTest(p);
    }

    waitForCondition(function() messageReceived == Social.providers.length,
                     next, "received messages from all workers");
  },
  testWorkerDisabling: function(next) {
    Social.enabled = false;
    is(Social.providers.length, gProviders.length, "providers still available");
    for (let p of Social.providers) {
      ok(!p.enabled, "provider disabled");
      ok(!p.getWorkerPort(), "worker disabled");
    }
    next();
  },

  testSingleWorkerEnabling: function(next) {
    // test that only one worker is enabled when we limit workers
    Services.prefs.setBoolPref("social.allowMultipleWorkers", false);
    Social.enabled = true;
    for (let p of Social.providers) {
      if (p == Social.provider) {
        ok(p.enabled, "primary provider enabled");
        let port = p.getWorkerPort();
        ok(port, "primary worker enabled");
        port.close();
      } else {
        ok(!p.enabled, "secondary provider is not enabled");
        ok(!p.getWorkerPort(), "secondary worker disabled");
      }
    }
    next();
  },

  testMultipleWorkerEnabling: function(next) {
    // test that all workers are enabled when we allow multiple workers
    Social.enabled = false;
    Services.prefs.setBoolPref("social.allowMultipleWorkers", true);
    Social.enabled = true;
    for (let p of Social.providers) {
      ok(p.enabled, "provider enabled");
      let port = p.getWorkerPort();
      ok(port, "worker enabled");
      port.close();
    }
    next();
  }
}
