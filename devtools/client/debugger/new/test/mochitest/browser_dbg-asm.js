/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that asm.js AOT can be disabled and debugging of the asm.js code
// is working.

add_task(function* () {
  const dbg = yield initDebugger("doc-asm.html");
  const { selectors: { getSelectedSource }, getState } = dbg;

  yield reload(dbg);

  // After reload() we are getting getSources notifiction for old sources,
  // using the debugger statement to really stop are reloaded page.
  yield waitForPaused(dbg);
  yield resume(dbg);

  yield waitForSources(dbg, "doc-asm.html", "asm.js");

  // Expand nodes and make sure more sources appear.
  is(findAllElements(dbg, "sourceNodes").length, 2);

  clickElement(dbg, "sourceArrow", 2);
  is(findAllElements(dbg, "sourceNodes").length, 4);

  selectSource(dbg, 'asm.js');

  yield addBreakpoint(dbg, "asm.js", 7);
  invokeInTab("runAsm");

  yield waitForPaused(dbg);
  assertPausedLocation(dbg, "asm.js", 7);
});
