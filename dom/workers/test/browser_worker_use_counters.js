/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const gHttpTestRoot = "https://example.com/browser/dom/workers/test/";

function unscream(s) {
  // Takes SCREAMINGCASE `s` and returns "Screamingcase".
  return s.charAt(0) + s.slice(1).toLowerCase();
}

function screamToCamel(s) {
  // Takes SCREAMING_CASE `s` and returns "screamingCase".
  const pascal = s.split("_").map(unscream).join("");
  return pascal.charAt(0).toLowerCase() + pascal.slice(1);
}

var check_use_counter_worker = async function (
  use_counter_name,
  worker_type,
  content_task
) {
  info(`checking ${use_counter_name} use counters for ${worker_type} worker`);

  let newTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.selectedTab = newTab;
  newTab.linkedBrowser.stop();

  // Hold on to the current values of the instrumentation we're
  // interested in.
  await Services.fog.testFlushAllChildren();
  let glean_before =
    Glean[`useCounterWorker${unscream(worker_type)}`][
      screamToCamel(use_counter_name)
    ].testGetValue();
  let glean_destructions_before =
    Glean.useCounter[
      `${worker_type.toLowerCase()}WorkersDestroyed`
    ].testGetValue();

  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    gHttpTestRoot + "file_use_counter_worker.html"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await content_task(gBrowser.selectedBrowser);

  // Tear down the page.
  let tabClosed = BrowserTestUtils.waitForTabClosing(newTab);
  gBrowser.removeTab(newTab);
  await tabClosed;

  // Grab data again and compare.
  // We'd like for this to be synchronous, but use counters are reported on
  // worker destruction which we don't directly observe.
  // So we check in a quick loop.
  await BrowserTestUtils.waitForCondition(async () => {
    await Services.fog.testFlushAllChildren();
    return (
      glean_before !=
      Glean[`useCounterWorker${unscream(worker_type)}`][
        screamToCamel(use_counter_name)
      ].testGetValue()
    );
  });
  let glean_after =
    Glean[`useCounterWorker${unscream(worker_type)}`][
      screamToCamel(use_counter_name)
    ].testGetValue();
  let glean_destructions_after =
    Glean.useCounter[
      `${worker_type.toLowerCase()}WorkersDestroyed`
    ].testGetValue();

  is(
    glean_after,
    glean_before + 1,
    `Glean counter ${use_counter_name} for ${worker_type} worker is correct.`
  );
  // There might be other workers created by prior tests get destroyed during
  // this tests.
  Assert.greater(
    glean_destructions_after,
    glean_destructions_before ?? 0,
    `Glean ${worker_type} worker counts are correct`
  );
};

add_task(async function test_dedicated_worker() {
  await check_use_counter_worker("CONSOLE_LOG", "DEDICATED", async browser => {
    await ContentTask.spawn(browser, {}, function () {
      return new Promise(resolve => {
        let worker = new content.Worker("file_use_counter_worker.js");
        worker.onmessage = function (e) {
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
    await ContentTask.spawn(browser, {}, function () {
      return new Promise(resolve => {
        let worker = new content.SharedWorker(
          "file_use_counter_shared_worker.js"
        );
        worker.port.onmessage = function (e) {
          if (e.data === "DONE") {
            resolve();
          }
        };
        worker.port.postMessage("RUN");
      });
    });
  });
});

add_task(async function test_shared_worker_microtask() {
  await check_use_counter_worker("CONSOLE_LOG", "SHARED", async browser => {
    await ContentTask.spawn(browser, {}, function () {
      return new Promise(resolve => {
        let worker = new content.SharedWorker(
          "file_use_counter_shared_worker_microtask.js"
        );
        worker.port.onmessage = function (e) {
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
    await ContentTask.spawn(browser, {}, function () {
      let waitForActivated = async function (registration) {
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
            content.navigator.serviceWorker.onmessage = function (e) {
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
