/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  runSocialTestWithProvider(gProviders, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function() {
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

   testMultipleWorkerEnabling: function(next) {
     // test that all workers are enabled when we allow multiple workers
     for (let p of Social.providers) {
       ok(p.enabled, "provider enabled");
       let port = p.getWorkerPort();
       ok(port, "worker enabled");
       port.close();
     }
     next();
   }
}
