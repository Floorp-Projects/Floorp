/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that eager evaluation skips breakpoints and debugger statements

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-strict.html");
  const { hud } = await getDebuggerSplitConsole(dbg);

  await addBreakpoint(dbg, "doc-strict.html", 15);
  setInputValue(hud, "strict()");
  await waitForEagerEvaluationResult(hud, `3`);

  setInputValue(hud, "debugger; 2");
  await waitForEagerEvaluationResult(hud, `2`);
});
