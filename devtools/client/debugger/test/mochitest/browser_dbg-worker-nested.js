/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that workers started by other workers show up in the debugger.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-nested-worker.html");

  await waitForThreadCount(dbg, 2);
  const workers = dbg.selectors.getThreads();
  ok(workers.length == 2, "Got two workers");
});
