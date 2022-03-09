/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests async stepping will step over await statements

"use strict";

add_task(async function test() {
  await pushPref("devtools.debugger.features.async-stepping", true);
  const dbg = await initDebugger("doc-async.html", "async.js");

  await selectSource(dbg, "async.js");
  await addBreakpoint(dbg, "async.js", 8);
  invokeInTab("main");

  await waitForPaused(dbg);
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 8);

  await stepOver(dbg);
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 9);
});
