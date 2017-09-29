/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * test pausing on an errored watch expression
 * assert that you can:
 * 1. resume
 * 2. still evalutate expressions
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
  pressKey(dbg, "End");
  type(dbg, input);
  pressKey(dbg, "Enter");
  await waitForDispatch(dbg, "EVALUATE_EXPRESSION");
}

add_task(async function() {
  const dbg = await initDebugger("doc-script-switching.html");

  await togglePauseOnExceptions(dbg, true, false);
  await addExpression(dbg, "location");

  const paused = waitForPaused(dbg);
  addExpression(dbg, "foo.bar");
  await paused;
  ok(dbg.selectors.hasWatchExpressionErrored(dbg.getState()));

  // Resume, and re-pause in the `foo.bar` exception
  resume(dbg);
  await waitForPaused(dbg);

  toggleExpression(dbg, 1);
  await waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
  is(findAllElements(dbg, "expressionNodes").length, 18);
});
