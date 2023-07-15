/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Test that breakpoints gets hit in sources generated from a sourcemap
// where the mappings are put in the `sections` property.

add_task(async function () {
  const dbg = await initDebugger("doc-sourcemaps.html", "xbundle.js");
  await selectSource(dbg, "xbundle.js");
  await waitForSelectedSource(dbg, "xbundle.js");

  await addBreakpoint(dbg, "xbundle.js", 5);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "xbundle.js").id, 5);

  // Switch to original source
  await dbg.actions.jumpToMappedSelectedLocation();

  info("Wait for the original file (xsource.js) to get selected");
  await waitForSelectedSource(dbg, "xsource.js");

  const originalSource = findSource(dbg, "xsource.js");
  assertPausedAtSourceAndLine(dbg, originalSource.id, 3);
});
