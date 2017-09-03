/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * tests the watch expressions component
 * 1. add watch expressions
 * 2. edit watch expressions
 * 3. delete watch expressions
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

add_task(function*() {
  const dbg = yield initDebugger("doc-script-switching.html");

  invokeInTab("firstCall");
  yield waitForPaused(dbg);

  yield addExpression(dbg, "f");
  is(getLabel(dbg, 1), "f");
  is(getValue(dbg, 1), "(unavailable)");

  yield editExpression(dbg, "oo");
  is(getLabel(dbg, 1), "foo()");

  // There is no "value" element for functions.
  assertEmptyValue(dbg, 1);

  yield addExpression(dbg, "location");
  is(getLabel(dbg, 2), "location");
  ok(getValue(dbg, 2).includes("Location"), "has a value");

  // can expand an expression
  toggleExpression(dbg, 2);
  yield waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");

  yield deleteExpression(dbg, "foo");
  yield deleteExpression(dbg, "location");

  is(findAllElements(dbg, "expressionNodes").length, 0);
});
