/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the debugger works when pausing on unload.

"use strict";

const TEST_URL_1 =
  "data:text/html," +
  encodeURI(
    `<script>window.addEventListener("unload", (event) => { debugger; });</script>`
  );

add_task(async function debuggerStatementOnUnload() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL_1);

  await addExpression(dbg, "event.type");
  is(getWatchExpressionLabel(dbg, 1), "event.type");
  is(getWatchExpressionValue(dbg, 1), "(unavailable)");

  info("Reloading the page should trigger the debugger statement on unload");
  const evaluated = waitForDispatch(dbg.store, "EVALUATE_EXPRESSIONS");
  const onReload = reload(dbg);

  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, TEST_URL_1).id, 1, 56);

  await evaluated;
  is(
    getWatchExpressionValue(dbg, 1),
    `"unload"`,
    "event.type evaluation does return the expected result"
  );

  // Verify that project search works while being paused on unload
  await openProjectSearch(dbg);
  await doProjectSearch(dbg, "unload", 1);

  info("Resume execution and wait for the page to complete its reload");
  await resume(dbg);
  await onReload;
});

// Note that inline exception doesn't work on single line <script>
const TEST_URL_2 =
  "data:text/html," +
  encodeURI(`<script>
window.addEventListener("unload", () => { throw new Error("Exception on unload") });
[].inlineException();
</script>`);

add_task(async function exceptionsOnUnload() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL_2);

  info("Enable pause on uncaught exception (but not for caught exception)");
  await togglePauseOnExceptions(dbg, true, false);

  await openProjectSearch(dbg);
  await doProjectSearch(dbg, "exception", 1);
  is(getExpandedResultsCount(dbg), 2);

  info("Reloading the page should trigger the debugger statement on unload");
  const onReload = reload(dbg);

  // Cover catching exception on unload
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, findSource(dbg, TEST_URL_2).id, 2, 49);

  // But also that previous inline exceptions are still visible
  await assertInlineExceptionPreview(dbg, 3, 4, {
    fields: [["<anonymous>", TEST_URL_2 + ":3"]],
    result: "TypeError: [].inlineException is not a function",
    expression: "inlineException",
  });

  // Verify that project search results are still displayed
  is(getExpandedResultsCount(dbg), 2);

  // Stop pausing on exception to prevent pausing on the inline exception on the load of the next page
  await togglePauseOnExceptions(dbg, false, false);

  info("Resume execution and wait for the page to complete its reload");
  await resume(dbg);
  await onReload;
});
