/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that worker modules do not crash when run in the debugger.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-module-worker.html");
  // reload the page
  await navigate(dbg, "doc-module-worker.html", "doc-module-worker.html");

  ok(true, "didn't crash");
  // TODO: bug 1816933 -- we can test actual functionality:
  //  await waitForThreadCount(dbg, 1);
  //  const workers = dbg.selectors.getThreads();
  //  ok(workers.length == 1, "Got one worker");
});
