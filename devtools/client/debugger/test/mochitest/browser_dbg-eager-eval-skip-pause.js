/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that eager evaluation skips breakpoints and debugger statements

add_task(async function() {
  await pushPref("devtools.webconsole.input.eagerEvaluation", true);

  const dbg = await initDebugger("doc-strict.html");
  const { hud } = await getDebuggerSplitConsole(dbg);

  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

  await addBreakpoint(dbg, "doc-strict.html", 15);
  setInputValue(hud, "strict()");
  await waitForEagerEvaluationResult(hud, `3`);

  setInputValue(hud, "debugger; 2");
  await waitForEagerEvaluationResult(hud, `2`);
});
