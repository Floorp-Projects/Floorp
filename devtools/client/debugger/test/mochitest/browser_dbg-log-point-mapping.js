/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests that expressions in log points are source mapped.
 */

"use strict";

add_task(async function () {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  await pushPref("devtools.debugger.map-scopes-enabled", true);

  const dbg = await initDebugger("doc-sourcemaps3.html", "test.js");

  const source = findSource(dbg, "test.js");
  await selectSource(dbg, "test.js");

  await getDebuggerSplitConsole(dbg);

  await addBreakpoint(dbg, "test.js", 6);
  await waitForBreakpoint(dbg, "test.js", 6);

  await dbg.actions.addBreakpoint(createLocation({ line: 5, source }), {
    logValue: "`value: ${JSON.stringify(test)}`",
    requiresMapping: true,
  });
  await waitForBreakpoint(dbg, "test.js", 5);

  invokeInTab("test");

  await waitForPaused(dbg);

  await hasConsoleMessage(dbg, "value:");
  const { value } = await findConsoleMessage(dbg, "value:");
  is(
    value,
    'value: ["b (30)","a","b (5)","z"]',
    "Variables in logpoint expression should be mapped"
  );
  await resume(dbg);
});
