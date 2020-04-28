/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * test pausing on an errored watch expression
 * assert that you can:
 * 1. resume
 * 2. still evalutate expressions
 * 3. expand properties
 */

const EXPRESSION_SELECTORS = {
  plusIcon: ".watch-expressions-pane button.plus",
  input: "input.input-expression"
};

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  await togglePauseOnExceptions(dbg, true, true);

  // add a good expression, 2 bad expressions, and another good one
  log(`Adding location`);
  await addExpression(dbg, "location");
  await addExpression(dbg, "foo.bar");
  await addExpression(dbg, "foo.batt");
  await addExpression(dbg, "2");
  // check the value of
  is(getValue(dbg, 2), "(unavailable)");
  is(getValue(dbg, 3), "(unavailable)");
  is(getValue(dbg, 4), 2);

  await toggleExpressionNode(dbg, 1);
  is(findAllElements(dbg, "expressionNodes").length, 37);
});

function getLabel(dbg, index) {
  return findElement(dbg, "expressionNode", index).innerText;
}

function getValue(dbg, index) {
  return findElement(dbg, "expressionValue", index).innerText;
}

async function addExpression(dbg, input) {
  const plusIcon = findElementWithSelector(dbg, EXPRESSION_SELECTORS.plusIcon);
  if (plusIcon) {
    plusIcon.click();
  }

  const evaluation = waitForDispatch(dbg, "EVALUATE_EXPRESSION");
  findElementWithSelector(dbg, EXPRESSION_SELECTORS.input).focus();
  type(dbg, input);
  pressKey(dbg, "Enter");
  await evaluation;
}
