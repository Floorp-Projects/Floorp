/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests pretty-printing a source that is currently paused.

add_task(async function() {
  const dbg = await initDebugger("doc-minified.html", "math.min.js");
  const thread = dbg.selectors.getCurrentThread();

  await selectSource(dbg, "math.min.js");
  await addBreakpoint(dbg, "math.min.js", 2);

  invokeInTab("arithmetic");
  await waitForPaused(dbg, "math.min.js");
  assertPausedLocation(dbg);

  clickElement(dbg, "prettyPrintButton");
  await waitForSelectedSource(dbg, "math.min.js:formatted");
  await waitForState(
    dbg,
    state => dbg.selectors.getSelectedFrame(thread).location.line == 18
  );
  assertPausedLocation(dbg);
  await waitForBreakpoint(dbg, "math.min.js:formatted", 18);
  await assertEditorBreakpoint(dbg, 18, true);

  await resume(dbg);
});
