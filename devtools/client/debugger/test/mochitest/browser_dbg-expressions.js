/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * tests the watch expressions component
 * 1. add watch expressions
 * 2. edit watch expressions
 * 3. delete watch expressions
 * 4. expanding properties when not paused
 */

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-script-switching.html");

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  await addExpression(dbg, "f");
  is(getWatchExpressionLabel(dbg, 1), "f");
  is(getWatchExpressionValue(dbg, 1), "(unavailable)");

  await editExpression(dbg, "oo");
  is(getWatchExpressionLabel(dbg, 1), "foo()");

  // There is no "value" element for functions.
  assertEmptyValue(dbg, 1);

  await addExpression(dbg, "location");
  is(getWatchExpressionLabel(dbg, 2), "location");
  ok(getWatchExpressionValue(dbg, 2).includes("Location"), "has a value");

  // can expand an expression
  await toggleExpressionNode(dbg, 2);

  is(findAllElements(dbg, "expressionNodes").length, 35);
  is(dbg.selectors.getExpressions(dbg.store.getState()).length, 2);

  await deleteExpression(dbg, "foo");
  await deleteExpression(dbg, "location");
  is(findAllElements(dbg, "expressionNodes").length, 0);
  is(dbg.selectors.getExpressions(dbg.store.getState()).length, 0);

  // Test expanding properties when the debuggee is active
  // Wait for full evaluation of the expressions in order to avoid having
  // mixed up code between the location being removed and the one being re-added
  const evaluated = waitForDispatch(dbg.store, "EVALUATE_EXPRESSIONS");
  await resume(dbg);
  await evaluated;

  await addExpression(dbg, "location");
  is(dbg.selectors.getExpressions(dbg.store.getState()).length, 1);

  is(findAllElements(dbg, "expressionNodes").length, 1);

  await toggleExpressionNode(dbg, 1);
  is(findAllElements(dbg, "expressionNodes").length, 34);

  await deleteExpression(dbg, "location");
  is(findAllElements(dbg, "expressionNodes").length, 0);

  info(
    "Test an expression calling a function with a breakpoint and a debugger statement, which shouldn't pause"
  );
  await addBreakpoint(dbg, "script-switching-01.js", 7);
  // firstCall will call secondCall from script-switching-02.js which has a debugger statement
  await addExpression(dbg, "firstCall()");
  is(getWatchExpressionLabel(dbg, 1), "firstCall()");
  is(getWatchExpressionValue(dbg, 1), "43", "has a value");
});

function assertEmptyValue(dbg, index) {
  const value = findElement(dbg, "expressionValue", index);
  if (value) {
    is(value.innerText, "");
    return;
  }

  is(value, null);
}
