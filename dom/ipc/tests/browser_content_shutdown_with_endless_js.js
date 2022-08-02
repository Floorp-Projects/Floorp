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

  let oldChildCount = Services.ppmm.childCount;
  info("Old process count: " + oldChildCount);

  let tabpromise = BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: url,
    waitForLoad: true,
    forceNewProcess: true,
  });

  let tab = await tabpromise;

  // It seems that the ppmm counter is racy wrt tabpromise.
  Services.tm.spinEventLoopUntil(
    "browser_content_shutdown_with_endless_js",
    () => Services.ppmm.childCount > oldChildCount
  );

  let newChildCount = Services.ppmm.childCount;
  info("New process count: " + newChildCount);

  ok(newChildCount == oldChildCount + 1, "Process created.");

  // Start the shutdown of the child process
  let tabClosed = BrowserTestUtils.waitForTabClosing(tab);
  BrowserTestUtils.removeTab(tab);
  ok(true, "removeTab");

  Services.tm.spinEventLoopUntil(
    "browser_content_shutdown_with_endless_js",
    () => Services.ppmm.childCount < newChildCount
  );

  info("New count: " + Services.ppmm.childCount);
  await tabClosed;

  ok(
    Services.ppmm.childCount == oldChildCount,
    "Shutdown of content process complete."
  );
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
