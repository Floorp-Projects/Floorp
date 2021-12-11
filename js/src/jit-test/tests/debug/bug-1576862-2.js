// |jit-test| skip-if: !('stackTest' in this)
// Failure to rewrap an exception in Completion::fromJSResult should be propagated.

var dbgGlobal = newGlobal({ newCompartment: true });
var dbg = new dbgGlobal.Debugger();
dbg.addDebuggee(this);

function test() {
  // Make this call's stack frame a debuggee, to ensure that
  // slowPathOnLeaveFrame runs when this frame exits. That calls
  // Completion::fromJSResult to capture this frame's completion value.
  dbg.getNewestFrame();

  // Throw from the non-debuggee compartment, to force Completion::fromJSResult
  // to rewrap the exception.
  dbgGlobal.assertEq(1,2);
}

stackTest(test, {
  // When the bug is fixed, the failure to rewrap the exception turns the throw
  // into a termination, so we won't get an exception.
  expectExceptionOnFailure: false
});
