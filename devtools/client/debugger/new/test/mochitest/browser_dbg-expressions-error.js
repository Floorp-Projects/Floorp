/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * test pausing on an errored watch expression
 * assert that you can:
 * 1. resume
 * 2. still evalutate expressions
 * 3. expand properties
 */

const expressionSelectors = {
  plusIcon: ".watch-expressions-pane button.plus",
  input: "input.input-expression"
};

function getLabel(dbg, index) {
  return findElement(dbg, "expressionNode", index).innerText;
}

function getValue(dbg, index) {
  return findElement(dbg, "expressionValue", index).innerText;
}

async function addExpression(dbg, input) {
  const plusIcon = findElementWithSelector(dbg, expressionSelectors.plusIcon);
  if (plusIcon) {
    plusIcon.click();
  }

  const evaluation = waitForDispatch(dbg, "EVALUATE_EXPRESSION");
  findElementWithSelector(dbg, expressionSelectors.input).focus();
  type(dbg, input);
  pressKey(dbg, "Enter");
  await evaluation;
}

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
