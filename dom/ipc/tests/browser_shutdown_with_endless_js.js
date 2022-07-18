/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const HANG_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_endless_js.html";

add_task(async () => {
  let tabpromise = BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: HANG_PAGE,
    waitForLoad: true,
    forceNewProcess: true,
  });

  await tabpromise;

  ok(true, "Tab with endless JS loaded.");

  // Now we will start a regular shutdown while our content script is running
  // an endless loop.
  //
  // If it does not get interrupted on shutdown, it will
  //
  // - cause a timeout while waiting for extension children to shutdown as
  //   those want to tell all content processes that they are going away
  //   but our test script is never returning the control to the event loop
  //   such that the ack we wait for will never arrive.
  //
  // - cause a timeout of SessionStore when flushing all windows for a
  //   similar reason.
  //
  // - finally cause an AsyncShutdown hang in profile-before-change as the
  //   child will never get to process the shutdown message itself, too.

  ok(true, "Sending quit.");
  Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
});
