function throwValue(value) {
  throw value;
}

// Test try-finally keep the exception stack.
function testFinally() {
  function f() {
    try {
      throwValue("exception-value");
    } finally {
      for (let i = 0; i < 100; ++i) {
        // OSR
      }
    }
  }

  let info = getExceptionInfo(f);
  assertEq(info.exception, "exception-value");
  assertEq(info.stack.includes("throwValue"), true);
}
testFinally();

// Test try-catch-finally keep the exception stack.
function testCatchFinally() {
  function f() {
    try {
      throw null;
    } catch {
     throwValue("exception-value");
    } finally {
      for (let i = 0; i < 100; ++i) {
        // OSR
      }
    }
  }

  let info = getExceptionInfo(f);
  assertEq(info.exception, "exception-value");
  assertEq(info.stack.includes("throwValue"), true);
}
testCatchFinally();
