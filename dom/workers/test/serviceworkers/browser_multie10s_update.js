"use strict";

const { classes: Cc, interfaces: Ci, results: Cr } = Components;

// Testing if 2 child processes are correctly managed when they both try to do
// an SW update.

const BASE_URI = "http://mochi.test:8888/browser/dom/workers/test/serviceworkers/";

add_task(function* test_update() {
  info("Setting the prefs to having multi-e10s enabled");
  yield SpecialPowers.pushPrefEnv({"set": [
    ["dom.ipc.processCount", 4],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]});

  let url = BASE_URI + "file_multie10s_update.html";

  info("Creating the first tab...");
  let tab1 = BrowserTestUtils.addTab(gBrowser, url);
  let browser1 = gBrowser.getBrowserForTab(tab1);
  yield BrowserTestUtils.browserLoaded(browser1);

  info("Creating the second tab...");
  let tab2 = BrowserTestUtils.addTab(gBrowser, url);
  let browser2 = gBrowser.getBrowserForTab(tab2);
  yield BrowserTestUtils.browserLoaded(browser2);

  let sw = BASE_URI + "server_multie10s_update.sjs";

  info("Let's start the test...");
  let status = yield ContentTask.spawn(browser1, sw, function(url) {
    // Registration of the SW
    return content.navigator.serviceWorker.register(url)

    // Activation
    .then(function(r) {
      return new content.window.Promise(resolve => {
        let worker = r.installing;
        worker.addEventListener('statechange', () => {
          if (worker.state === 'installed') {
            resolve(true);
          }
        });
      });
    })

    // Waiting for the result.
    .then(() => {
      return new content.window.Promise(resolve => {
        let results = [];
        let bc = new content.window.BroadcastChannel('result');
        bc.onmessage = function(e) {
          results.push(e.data);
          if (results.length != 2) {
            return;
          }

          resolve(results[0] + results[1]);
        }

        // Let's inform the tabs.
        bc = new content.window.BroadcastChannel('start');
        bc.postMessage('go');
      });
    });
  });

  if (status == 0) {
    ok(false, "both succeeded. This is wrong.");
  } else if (status == 1) {
    ok(true, "one succeded, one failed. This is good.");
  } else {
    ok(false, "both failed. This is definitely wrong.");
  }

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
