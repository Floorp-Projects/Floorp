/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EMPTY_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_dummy.html";

const HANG_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_endless_js.html";

function pushPref(name, val) {
  return SpecialPowers.pushPrefEnv({ set: [[name, val]] });
}

async function createAndShutdownContentProcess(url) {
  info("Create and shutdown a content process for " + url);

  // Launch a new process and load url. Sets up a promise that will resolve
  // on shutdown.
  let browserParentDestroyed = PromiseUtils.defer();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      opening: url,
      waitForLoad: true,
      forceNewProcess: true,
    },
    async function(otherBrowser) {
      let remoteTab = otherBrowser.frameLoader.remoteTab;

      ok(true, "Content process created.");

      browserParentDestroyed.resolve(
        TestUtils.topicObserved(
          "ipc:browser-destroyed",
          subject => subject === remoteTab
        )
      );

      // Give the content process some extra time before we start its shutdown.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => setTimeout(resolve, 50));

      // withNewTab will start the shutdown of the child process for us
    }
  );

  // Now wait for it to really shut down.
  // If the HANG_PAGE JS is not canceled we will hang here.
  await browserParentDestroyed.promise;

  // If we do not hang and get here, we are fine.
  ok(true, "Shutdown of content process.");
}

add_task(async () => {
  // This test is only relevant in e10s.
  if (!gMultiProcessBrowser) {
    ok(true, "We are not in multiprocess mode, skipping test.");
    return;
  }

  await pushPref("dom.abort_script_on_child_shutdown", true);

  // Ensure the process cache cannot interfere.
  pushPref("dom.ipc.processPreload.enabled", false);
  // Ensure we have no cached processes from previous tests.
  Services.ppmm.releaseCachedProcesses();

  // First let's do a dry run that should always succeed.
  await createAndShutdownContentProcess(EMPTY_PAGE);

  // Now we will start a shutdown of our content process while our content
  // script is running an endless loop.
  //
  // If the JS does not get interrupted on shutdown, it will cause this test
  // to hang.
  await createAndShutdownContentProcess(HANG_PAGE);
});
