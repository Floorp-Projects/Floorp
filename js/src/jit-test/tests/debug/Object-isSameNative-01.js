// Test that isSameNative will accept a CCW as an argument.

var g = newGlobal({ newCompartment: true });
var dbg = Debugger();
var gdbg = dbg.addDebuggee(g);

var g2 = newGlobal({ newCompartment: true });

assertEq(gdbg.getProperty("print").return.isSameNative(g2.print), true);
