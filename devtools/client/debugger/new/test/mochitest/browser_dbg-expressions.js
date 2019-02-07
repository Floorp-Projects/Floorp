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

function getLabel(dbg, index) {
  return findElement(dbg, "expressionNode", index).innerText;
}

function getValue(dbg, index) {
  return findElement(dbg, "expressionValue", index).innerText;
}

function assertEmptyValue(dbg, index) {
  const value = findElement(dbg, "expressionValue", index);
  if (value) {
    is(value.innerText, "");
    return;
  }

  is(value, null);
}

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  await addExpression(dbg, "f");
  is(getLabel(dbg, 1), "f");
  is(getValue(dbg, 1), "(unavailable)");

  await editExpression(dbg, "oo");
  is(getLabel(dbg, 1), "foo()");

  // There is no "value" element for functions.
  assertEmptyValue(dbg, 1);

  await addExpression(dbg, "location");
  is(getLabel(dbg, 2), "location");
  ok(getValue(dbg, 2).includes("Location"), "has a value");

  // can expand an expression
  await toggleExpressionNode(dbg, 2);

  await deleteExpression(dbg, "foo");
  await deleteExpression(dbg, "location");
  is(findAllElements(dbg, "expressionNodes").length, 0);

  // Test expanding properties when the debuggee is active
  await resume(dbg);
  await addExpression(dbg, "location");

  is(findAllElements(dbg, "expressionNodes").length, 1);

  await toggleExpressionNode(dbg, 1);
  is(findAllElements(dbg, "expressionNodes").length, 34);

  await deleteExpression(dbg, "location");
  is(findAllElements(dbg, "expressionNodes").length, 0);
});
