/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// This test focuses on verifying that the conditional breakpoint are trigerred.

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple2.js");

  await selectSource(dbg, "simple2.js");

  info("Set condition `x === 1` in the foo function, and pause");
  await setConditionalBreakpoint(dbg, 5, "x === 1");

  invokeInTab("foo", /* x */ 1);
  await waitForPaused(dbg);
  ok(true, "Conditional breakpoint was hit");
  await resume(dbg);

  await removeBreakpoint(dbg, findSource(dbg, "simple2.js").id, 5);

  info("Set condition `x === 2` in the foo function, and do not pause");
  await setConditionalBreakpoint(dbg, 5, "x == 2");

  invokeInTab("foo", /* x */ 1);
  // Let some time for a leftover breakpoint to be hit
  await wait(500);
  assertNotPaused(dbg);

  await removeBreakpoint(dbg, findSource(dbg, "simple2.js").id, 5);

  info("Set condition `foo(` (syntax error), and pause on the exception");
  await setConditionalBreakpoint(dbg, 5, "foo(");

  invokeInTab("main");
  await waitForPaused(dbg);
  let whyPaused = dbg.win.document.querySelector(".why-paused").innerText;
  is(
    whyPaused,
    "Error with conditional breakpoint\nexpected expression, got end of script"
  );
  await resume(dbg);
  assertNotPaused(dbg);

  info(
    "Retrigger the same breakpoint with pause on exception enabled and ensure it still reports the exception and only once"
  );
  await togglePauseOnExceptions(dbg, true, false);

  invokeInTab("main");
  await waitForPaused(dbg);
  whyPaused = dbg.win.document.querySelector(".why-paused").innerText;
  is(
    whyPaused,
    "Error with conditional breakpoint\nexpected expression, got end of script"
  );
  await resume(dbg);

  // Let some time for a duplicated breakpoint to be hit
  await wait(500);
  assertNotPaused(dbg);

  await removeBreakpoint(dbg, findSource(dbg, "simple2.js").id, 5);
});

async function setConditionalBreakpoint(dbg, index, condition) {
  // Make this work with either add or edit menu items
  const { addConditionItem, editConditionItem } = selectors;
  const selector = `${addConditionItem},${editConditionItem}`;
  rightClickElement(dbg, "gutter", index);
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, selector);
  const dispatched = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  typeInPanel(dbg, condition);
  await dispatched;
  await waitForCondition(dbg, condition);
}
