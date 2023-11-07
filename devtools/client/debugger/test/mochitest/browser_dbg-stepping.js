/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// This test can be really slow on debug platforms
requestLongerTimeout(5);

add_task(async function test() {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  const dbg = await initDebugger(
    "big-sourcemap.html",
    "bundle.js",
    "step-in-test.js"
  );
  invokeInTab("hitDebugStatement");
  // Bug 1829860 - The sourcemap for this file is broken and we couldn't resolve the paused location
  // to the related original file. The debugger fallbacks to the generated source,
  // but that highlights a bug in the example page.
  await waitForPaused(dbg, "bundle.js");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "bundle.js").id, 52411);

  await stepIn(dbg);

  const whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(whyPaused, `Paused while stepping`);

  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);
  await stepIn(dbg);

  // Note that we are asserting against an original source here,
  // See earlier comment about paused in bundle.js
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "step-in-test.js").id, 7679);
});
