function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

function getValue(dbg, index) {
  return findElement(dbg, "scopeValue", index).innerText;
}

async function testReturnValue(dbg, val) {
  invokeInTab("return_something", val);
  await waitForPaused(dbg);

  // "Step in" 3 times to get to the point where the debugger can
  // see the return value.
  await stepIn(dbg);
  await stepIn(dbg);

  is(getLabel(dbg, 1), "return_something", "check for return_something");

  // We don't show "undefined" but we do show other falsy values.
  let label = getLabel(dbg, 2);
  if (val === "undefined") {
    ok(label !== "<return>", "do not show <return> for undefined");
  } else {
    is(label, "<return>", "check for <return>");
    // The "uneval" call here gives us the way one would write `val` as
    // a JavaScript expression, similar to the way the debugger
    // displays values, so this test works when `val` is a string.
    is(getValue(dbg, 2), uneval(val), `check value is ${uneval(val)}`);
  }

  await resume(dbg);
  assertNotPaused(dbg);
}

async function testThrowValue(dbg, val) {
  invokeInTab("throw_something", val);
  await waitForPaused(dbg);

  // "Step in" once to get to the point where the debugger can see the
  // exception.
  await stepIn(dbg);

  is(getLabel(dbg, 1), "callee", "check for callee");
  is(getLabel(dbg, 2), "<exception>", "check for <exception>");
  // The "uneval" call here gives us the way one would write `val` as
  // a JavaScript expression, similar to the way the debugger
  // displays values, so this test works when `val` is a string.
  is(getValue(dbg, 2), uneval(val), `check exception is ${uneval(val)}`);

  await resume(dbg);
  await waitForPaused(dbg);
  await resume(dbg);
  assertNotPaused(dbg);
}

add_task(async function() {
  const dbg = await initDebugger("doc-return-values.html");
  await togglePauseOnExceptions(dbg, true, true);

  await testReturnValue(dbg, "to sender");
  await testThrowValue(dbg, "a fit");
});
