"use strict";

// Testing if 2 child processes are correctly managed when they both try to do
// an SW update.

const BASE_URI =
 "http://mochi.test:8888/browser/dom/serviceworkers/test/isolated/multi-e10s-update/";

add_task(async function test_update() {
  info("Setting the prefs to having multi-e10s enabled");
  await SpecialPowers.pushPrefEnv({"set": [
    ["dom.ipc.processCount", 4],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]});

  let url = BASE_URI + "file_multie10s_update.html";

  info("Creating the first tab...");
  let tab1 = BrowserTestUtils.addTab(gBrowser, url);
  let browser1 = gBrowser.getBrowserForTab(tab1);
  await BrowserTestUtils.browserLoaded(browser1);

  info("Creating the second tab...");
  let tab2 = BrowserTestUtils.addTab(gBrowser, url);
  let browser2 = gBrowser.getBrowserForTab(tab2);
  await BrowserTestUtils.browserLoaded(browser2);

  let sw = BASE_URI + "server_multie10s_update.sjs";

  info("Let's make sure there are no existing registrations...");
  let existingCount = await ContentTask.spawn(browser1, null, async function() {
    const regs = await content.navigator.serviceWorker.getRegistrations();
    return regs.length;
  });
  is(existingCount, 0, "Previous tests should have cleaned up!");

  info("Let's start the test...");
  /* eslint-disable no-shadow */
  let status = await ContentTask.spawn(browser1, sw, function(url) {
    // Let the SW be served immediately once by triggering a relase immediately.
    // We don't need to await this.  We do this from a frame script because
    // it has fetch.
    content.fetch(url + "?release");

    // Registration of the SW
    return content.navigator.serviceWorker.register(url)

    // Activation
    .then(function(r) {
      content.registration = r;
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
      return new content.window.Promise(resolveResults => {
        // Once both updates have been issued and a single update has failed, we
        // can tell the .sjs to release a single copy of the SW script.
        let updateCount = 0;
        const uc = new content.window.BroadcastChannel('update');
        // This promise tracks the updates tally.
        const updatesIssued = new Promise(resolveUpdatesIssued => {
          uc.onmessage = function(e) {
            updateCount++;
            console.log("got update() number", updateCount);
            if (updateCount === 2) {
              resolveUpdatesIssued();
            }
          };
        });

        let results = [];
        const rc = new content.window.BroadcastChannel('result');
        // This promise resolves when an update has failed.
        const oneFailed = new Promise(resolveOneFailed => {
          rc.onmessage = function(e) {
            console.log("got result", e.data);
            results.push(e.data);
            if (e.data === 1) {
              resolveOneFailed();
            }
            if (results.length != 2) {
              return;
            }

            resolveResults(results[0] + results[1]);
          }
        });

        Promise.all([updatesIssued, oneFailed]).then(() => {
          console.log("releasing update");
          content.fetch(url + "?release").catch((ex) => {
            console.error("problem releasing:", ex);
          });
        });

        // Let's inform the tabs.
        const sc = new content.window.BroadcastChannel('start');
        sc.postMessage('go');
      });
    });
  });
  /* eslint-enable no-shadow */

  if (status == 0) {
    ok(false, "both succeeded. This is wrong.");
  } else if (status == 1) {
    ok(true, "one succeded, one failed. This is good.");
  } else {
    ok(false, "both failed. This is definitely wrong.");
  }

  // let's clean up the registration and get the fetch count.  The count
  // should be 1 for the initial fetch and 1 for the update.
  /* eslint-disable no-shadow */
  const count = await ContentTask.spawn(browser1, sw, async function(url) {
    // We stored the registration on the frame script's wrapper, hence directly
    // accesss content without using wrappedJSObject.
    await content.registration.unregister();
    const { count } =
      await content.fetch(url + "?get-and-clear-count").then(r => r.json());
    return count;
  });
  /* eslint-enable no-shadow */
  is(count, 2, "SW should have been fetched only twice");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
