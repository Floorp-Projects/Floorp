/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const TEST_COM_URI =
  URL_ROOT_COM + "examples/doc_dbg-fission-frame-sources.html";

add_task(async function() {
  // Load a test page with a remote frame:
  // simple1.js is imported by the main page. simple2.js comes from the remote frame.
  const dbg = await initDebuggerWithAbsoluteURL(
    TEST_COM_URI,
    "simple1.js",
    "simple2.js"
  );
  const {
    selectors: { getSelectedSource, getIsPaused, getCurrentThread },
  } = dbg;

  // Add breakpoint within the iframe, which is hit early on load
  await selectSource(dbg, "simple2.js");
  await addBreakpoint(dbg, "simple2.js", 7);

  const onBreakpoint = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  info("Reload the page to hit the breakpoint on load");
  await reload(dbg);
  await onBreakpoint
  await waitForSelectedSource(dbg, "simple2.js");

  ok(getSelectedSource().url.includes("simple2.js"), "Selected source is simple2.js");
  assertPausedLocation(dbg);
  assertDebugLine(dbg, 7);

  await stepIn(dbg);
  assertDebugLine(dbg, 7);
  assertPausedLocation(dbg);

  // We can't used `stepIn` helper as this last step will resume
  // and the helper is expecting to pause again
  await dbg.actions.stepIn(getThreadContext(dbg));
  ok(!isPaused(dbg), "Stepping in two times resumes");

  await dbg.toolbox.closeToolbox();
});
