/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Test the watch expressions "refresh" button:
 * - hidden when no expression is available
 * - visible with one or more expressions
 * - updates expressions values after clicking on it
 * - disappears when all expressions are removed
 */

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  ok(
    !getRefreshExpressionsElement(dbg),
    "No refresh button is displayed when there are no watch expressions"
  );

  await addExpression(dbg, "someVariable");

  ok(
    getRefreshExpressionsElement(dbg),
    "Refresh button is displayed after adding a watch expression"
  );

  is(getLabel(dbg, 1), "someVariable", "Watch expression was added");
  is(getValue(dbg, 1), "(unavailable)", "Watch expression has no value");

  info("Switch to the console and update the value of the watched variable");
  const { hud } = await dbg.toolbox.selectTool("webconsole");
  await evaluateExpressionInConsole(hud, "var someVariable = 1");

  info("Switch back to the debugger");
  await dbg.toolbox.selectTool("jsdebugger");

  is(getLabel(dbg, 1), "someVariable", "Watch expression is still available");
  is(getValue(dbg, 1), "(unavailable)", "Watch expression still has no value");

  info(
    "Click on the watch expression refresh button and wait for the " +
      "expression to update."
  );
  const refreshed = waitForDispatch(dbg, "EVALUATE_EXPRESSIONS");
  await clickElement(dbg, "expressionRefresh");
  await refreshed;

  is(getLabel(dbg, 1), "someVariable", "Watch expression is still available");
  is(getValue(dbg, 1), "1", "Watch expression value has been updated");

  await deleteExpression(dbg, "someVariable");

  ok(
    !getRefreshExpressionsElement(dbg),
    "The refresh button is no longer displayed after removing watch expressions"
  );
});

function getLabel(dbg, index) {
  return findElement(dbg, "expressionNode", index).innerText;
}

function getValue(dbg, index) {
  return findElement(dbg, "expressionValue", index).innerText;
}

function getRefreshExpressionsElement(dbg) {
  return findElement(dbg, "expressionRefresh", 1);
}
