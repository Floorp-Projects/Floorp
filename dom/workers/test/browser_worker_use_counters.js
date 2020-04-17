/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const gHttpTestRoot = "https://example.com/browser/dom/workers/test/";

function grabHistogramsFromContent(
  use_counter_name,
  worker_type,
  counter_before = null
) {
  let telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(
    Ci.nsITelemetry
  );
  let gather = () => {
    let snapshots;
    if (Services.appinfo.browserTabsRemoteAutostart) {
      snapshots = telemetry.getSnapshotForHistograms("main", false).content;
    } else {
      snapshots = telemetry.getSnapshotForHistograms("main", false).parent;
    }
    let checkedGet = probe => {
      return snapshots[probe] ? snapshots[probe].sum : 0;
    };
    return [
      checkedGet(`USE_COUNTER2_${use_counter_name}_${worker_type}_WORKER`),
      checkedGet(`${worker_type}_WORKER_DESTROYED`),
    ];
  };
  return BrowserTestUtils.waitForCondition(() => {
    return counter_before != gather()[0];
  }).finally(gather);
}

var check_use_counter_worker = async function(
  use_counter_name,
  worker_type,
  content_task
) {
  info(`checking ${use_counter_name} use counters for ${worker_type} worker`);

  let newTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.selectedTab = newTab;
  newTab.linkedBrowser.stop();

  // Hold on to the current values of the telemetry histograms we're
  // interested in.
  let [histogram_before, destructions_before] = await grabHistogramsFromContent(
    use_counter_name,
    worker_type
  );

  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    gHttpTestRoot + "file_use_counter_worker.html"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await content_task(gBrowser.selectedBrowser);

  // Tear down the page.
  let tabClosed = BrowserTestUtils.waitForTabClosing(newTab);
  gBrowser.removeTab(newTab);
  await tabClosed;

  // Grab histograms again and compare.
  let [histogram_after, destructions_after] = await grabHistogramsFromContent(
    use_counter_name,
    worker_type,
    histogram_before
  );

  is(
    histogram_after,
    histogram_before + 1,
    `histogram ${use_counter_name} counts for ${worker_type} worker are correct`
  );
  // There might be other workers created by prior tests get destroyed during
  // this tests.
  ok(
    destructions_after > destructions_before,
    `${worker_type} worker counts are correct`
  );
};

add_task(async function test_dedicated_worker() {
  await check_use_counter_worker("CONSOLE_LOG", "DEDICATED", async browser => {
    await ContentTask.spawn(browser, {}, function() {
      return new Promise(resolve => {
        let worker = new content.Worker("file_use_counter_worker.js");
        worker.onmessage = function(e) {
          if (e.data === "DONE") {
            worker.terminate();
            resolve();
          }
        };
      });
    });
  });
});

add_task(async function test_shared_worker() {
  await check_use_counter_worker("CONSOLE_LOG", "SHARED", async browser => {
    await ContentTask.spawn(browser, {}, function() {
      return new Promise(resolve => {
        let worker = new content.SharedWorker(
          "file_use_counter_shared_worker.js"
        );
        worker.port.onmessage = function(e) {
          if (e.data === "DONE") {
            resolve();
          }
        };
        worker.port.postMessage("RUN");
      });
    });
  });
});

add_task(async function test_service_worker() {
  await check_use_counter_worker("CONSOLE_LOG", "SERVICE", async browser => {
    await ContentTask.spawn(browser, {}, function() {
      let waitForActivated = async function(registration) {
        return new Promise(resolve => {
          let worker =
            registration.installing ||
            registration.waiting ||
            registration.active;
          if (worker.state === "activated") {
            resolve(worker);
            return;
          }

          worker.addEventListener("statechange", function onStateChange() {
            if (worker.state === "activated") {
              worker.removeEventListener("statechange", onStateChange);
              resolve(worker);
            }
          });
        });
      };

      return new Promise(resolve => {
        content.navigator.serviceWorker
          .register("file_use_counter_service_worker.js")
          .then(async registration => {
            content.navigator.serviceWorker.onmessage = function(e) {
              if (e.data === "DONE") {
                registration.unregister().then(resolve);
              }
            };
            let worker = await waitForActivated(registration);
            worker.postMessage("RUN");
          });
      });
    });
  });
});
