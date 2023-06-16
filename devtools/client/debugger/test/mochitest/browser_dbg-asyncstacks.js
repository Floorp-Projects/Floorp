/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that async stacks include the async separator

"use strict";

add_task(async function () {
  pushPref("devtools.debugger.features.async-captured-stacks", true);
  const dbg = await initDebugger("doc-frames-async.html");

  invokeInTab("main");
  await waitForPaused(dbg);

  is(findElement(dbg, "frame", 1).innerText, "sleep\ndoc-frames-async.html:13");
  is(
    findElement(dbg, "frame", 2).innerText,
    "async\nmain\ndoc-frames-async.html:17"
  );
});
