/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-asm.html");
  await reload(dbg);

  // After reload() we are getting getSources notifiction for old sources,
  // using the debugger statement to really stop are reloaded page.
  await waitForPaused(dbg);
  await resume(dbg);

  await waitForSources(dbg, "doc-asm.html", "asm.js");

  // Make sure sources appear.
  is(findAllElements(dbg, "sourceNodes").length, 3);

  await selectSource(dbg, "asm.js");

  await addBreakpoint(dbg, "asm.js", 7);
  invokeInTab("runAsm");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "asm.js").id, 7);
});
