/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests that log points in a worker are correctly logged to the console
 */

"use strict";

add_task(async function () {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);

  const dbg = await initDebugger("doc-windowless-workers.html");

  await waitForSource(dbg, "simple-worker.js");
  await selectSource(dbg, "simple-worker.js");

  await altClickElement(dbg, "gutter", 4);
  await waitForBreakpoint(dbg, "simple-worker.js", 4);

  await getDebuggerSplitConsole(dbg);
  await hasConsoleMessage(dbg, "timer");
  const { link } = await findConsoleMessage(dbg, "timer");
  is(
    link,
    "simple-worker.js:4:9",
    "message should include the url and linenumber"
  );
});
