// Test calling a function using a previously captured stack as an async stack.

function getAsyncStack() {
  return saveStack();
}

// asyncCause may contain non-ASCII characters.
let testAsyncCause = "Tes" + String.fromCharCode(355) + "String";

callFunctionWithAsyncStack(function asyncCallback() {
  let stack = saveStack();

  assertEq(stack.functionDisplayName, "asyncCallback");
  assertEq(stack.parent, null);
  assertEq(stack.asyncCause, null);

  assertEq(stack.asyncParent.functionDisplayName, "getAsyncStack");
  assertEq(stack.asyncParent.asyncCause, testAsyncCause);
  assertEq(stack.asyncParent.asyncParent, null);

  assertEq(stack.asyncParent.parent.asyncCause, null);
  assertEq(stack.asyncParent.parent.asyncParent, null);
  assertEq(stack.asyncParent.parent.parent, null);
}, getAsyncStack(), testAsyncCause);
