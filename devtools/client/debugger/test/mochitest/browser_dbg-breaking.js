/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests the breakpoints are hit in various situations.

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html");
  const {
    selectors: { getSelectedSource },
  } = dbg;

  await selectSource(dbg, "doc-scripts.html");

  // Make sure we can set a top-level breakpoint and it will be hit on
  // reload.
  await addBreakpoint(dbg, "doc-scripts.html", 21);

  const onReloaded = reload(dbg, "doc-scripts.html");

  await waitForPaused(dbg);

  let whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(whyPaused, "Paused on breakpoint");

  assertPausedAtSourceAndLine(dbg, findSource(dbg, "doc-scripts.html").id, 21);
  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded;

  info("Create an eval script that pauses itself.");
  invokeInTab("doEval");
  await waitForPaused(dbg);
  const source = getSelectedSource();
  ok(!source.url, "It is an eval source");
  assertPausedAtSourceAndLine(dbg, source.id, 2);

  whyPaused = await waitFor(
    () => dbg.win.document.querySelector(".why-paused")?.innerText
  );
  is(whyPaused, "Paused on debugger statement");

  await resume(dbg);

  await addBreakpoint(dbg, source, 5);
  invokeInTab("evaledFunc");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 5);

  await resume(dbg);
});
