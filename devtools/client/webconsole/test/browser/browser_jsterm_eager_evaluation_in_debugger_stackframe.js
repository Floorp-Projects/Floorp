/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that eager evaluation works as expected when paused in the debugger.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
<script>
var x = "global";

function pauseInDebugger(param) {
  let x = "local";
  debugger;
}

</script>
`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

  setInputValue(hud, "x");
  await waitForEagerEvaluationResult(hud, `"global"`);

  info("Open Debugger");
  await openDebugger();
  const dbg = createDebuggerContext(toolbox);

  info("Pause in Debugger");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.pauseInDebugger("myParam");
  });
  await pauseDebugger(dbg);

  info("Opening Console");
  await toolbox.selectTool("webconsole");

  info("Check that the parameter is eagerly evaluated as expected");
  setInputValue(hud, "param");
  await waitForEagerEvaluationResult(hud, `"myParam"`);

  setInputValue(hud, "x");
  await waitForEagerEvaluationResult(hud, `"local"`);

  await resume(dbg);
});
