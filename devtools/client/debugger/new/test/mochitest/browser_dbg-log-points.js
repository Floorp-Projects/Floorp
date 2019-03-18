/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests that log points are correctly logged to the console
 */

add_task(async function() {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = await initDebugger("doc-script-switching.html", "switching-01");

  const source = findSource(dbg, "switching-01")
  await selectSource(dbg, "switching-01");

  await getDebuggerSplitConsole(dbg);

  await dbg.actions.addBreakpoint(
    { line: 5, sourceId: source.id },
    { logValue: "'a', 'b', 'c'" }
  );
  invokeInTab("firstCall");
  await waitForPaused(dbg);

  await hasConsoleMessage(dbg, "a b c");
  const { link, value } = await findConsoleMessage(dbg, "a b c");
  is(link, "script-switching-01.js:5:2", "logs should have the relevant link");
  is(value, "a b c", "logs should have multiple values");
});
