/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests that log points are correctly logged to the console
 */

"use strict";

add_task(async function () {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  const source = findSource(dbg, "script-switching-01.js");
  await selectSource(dbg, "script-switching-01.js");

  await getDebuggerSplitConsole(dbg);

  await altClickElement(dbg, "gutter", 7);
  await waitForBreakpoint(dbg, "script-switching-01.js", 7);

  await dbg.actions.addBreakpoint(
    getContext(dbg),
    createLocation({ line: 8, source }),
    { logValue: "'a', 'b', 'c'" }
  );

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  await hasConsoleMessage(dbg, "a b c");
  await hasConsoleMessage(dbg, "firstCall");

  const { link, value } = await findConsoleMessage(dbg, "a b c");
  is(link, "script-switching-01.js:8:2", "logs should have the relevant link");
  is(value, "a b c", "logs should have multiple values");
});
