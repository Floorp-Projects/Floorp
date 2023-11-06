/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  const dbg = await initDebugger(
    "doc-sourcemaps.html",
    "entry.js",
    "output.js",
    "times2.js",
    "opts.js"
  );

  await selectSource(dbg, "times2.js");
  await addBreakpoint(dbg, "times2.js", 2);

  invokeInTab("keepMeAlive");
  await waitForPausedInOriginalFileAndToggleMapScopes(dbg, "times2.js");

  info("Test previewing in the original location");
  await assertPreviews(dbg, [
    { line: 2, column: 10, result: 4, expression: "x" },
  ]);

  info("Test previewing in the generated location");
  await dbg.actions.jumpToMappedSelectedLocation();
  await waitForSelectedSource(dbg, "bundle.js");
  await assertPreviews(dbg, [
    { line: 70, column: 11, result: 4, expression: "x" },
  ]);

  info("Test that you can not preview in another original file");
  await selectSource(dbg, "output.js");
  await hoverAtPos(dbg, { line: 2, column: 17 });
  await assertNoTooltip(dbg);
});
