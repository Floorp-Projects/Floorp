function throwValue(value) {
  throw value;
}

// Test for-of loops keep the exception stack.
function testForOfLoop() {
  function f() {
    for (let _ of [null]) {
      throwValue("exception-value");
    }
  }

  let info = getExceptionInfo(f);
  assertEq(info.exception, "exception-value");
  assertEq(info.stack.includes("throwValue"), true);
}
testForOfLoop();
