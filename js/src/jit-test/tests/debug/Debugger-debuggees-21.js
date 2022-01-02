// Errors adding globals in addAllGlobalsAsDebuggees should be reported.

// The exception that might be thrown in this test reflects our inability
// to change compartments to debug mode while they have frames on the
// stack. If we run this test with --debugjit, it won't throw an error at
// all, since all compartments are already in debug mode. So, pass if the
// script completes normally, or throws an appropriate exception.
try {
  newGlobal().eval("(new Debugger).addAllGlobalsAsDebuggees();");
} catch (ex) {
  assertEq(!!(''+ex).match(/can't start debugging: a debuggee script is on the stack/), true);
}
