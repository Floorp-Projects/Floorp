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
  input: "input.input-expression"
};

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

function toggleExpression(dbg, index) {
  findElement(dbg, "expressionNode", index).click();
}

async function addExpression(dbg, input) {
  info("Adding an expression");
  findElementWithSelector(dbg, expressionSelectors.input).focus();
  type(dbg, input);
  pressKey(dbg, "Enter");
  await waitForDispatch(dbg, "EVALUATE_EXPRESSION");
}

async function editExpression(dbg, input) {
  info("updating the expression");
  dblClickElement(dbg, "expressionNode", 1);
  // Position cursor reliably at the end of the text.
  const evaluation = waitForDispatch(dbg, "EVALUATE_EXPRESSION");
  pressKey(dbg, "End");
  type(dbg, input);
  pressKey(dbg, "Enter");
  await evaluation;
}

/*
 * When we add a bad expression, we'll pause,
 * resume, and wait for the expression to finish being evaluated.
 */
async function addBadExpression(dbg, input) {
  const evaluation = waitForDispatch(dbg, "EVALUATE_EXPRESSION");

  findElementWithSelector(dbg, expressionSelectors.input).focus();
  type(dbg, input);
  pressKey(dbg, "Enter");

  await waitForPaused(dbg);

  ok(dbg.selectors.isEvaluatingExpression(dbg.getState()));
  await resume(dbg);
  await evaluation;
}

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  await togglePauseOnExceptions(dbg, true, false);

  // add a good expression, 2 bad expressions, and another good one
  await addExpression(dbg, "location");
  await addBadExpression(dbg, "foo.bar");
  await addBadExpression(dbg, "foo.batt");
  await addExpression(dbg, "2");

  // check the value of
  is(getValue(dbg, 2), "(unavailable)");
  is(getValue(dbg, 3), "(unavailable)");
  is(getValue(dbg, 4), 2);

  toggleExpression(dbg, 1);
  await waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
  is(findAllElements(dbg, "expressionNodes").length, 20);
});
