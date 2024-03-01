var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

assertEq(gdbg.getProperty("print").return.isSameNativeWithJitInfo(print), true);
assertEq(gdbg.getProperty("print").return.isSameNativeWithJitInfo(newGlobal), false);

// FakeDOMObject's accessor shares the single native functions, with
// different JSJitInfo for each.

gdbg.executeInGlobal(`
var fun1 = Object.getOwnPropertyDescriptor(FakeDOMObject.prototype, "x").get;
var fun2 = Object.getOwnPropertyDescriptor(FakeDOMObject.prototype, "slot").get;
`);

var g_fun1 = gdbg.executeInGlobal("fun1").return;
var g_fun2 = gdbg.executeInGlobal("fun2").return;

var fun1 = Object.getOwnPropertyDescriptor(FakeDOMObject.prototype, "x").get;
var fun2 = Object.getOwnPropertyDescriptor(FakeDOMObject.prototype, "slot").get;

// isSameNative doesn't distinguish between fun1 and fun2.
assertEq(g_fun1.isSameNative(fun1), true);
assertEq(g_fun1.isSameNative(fun2), true);
assertEq(g_fun2.isSameNative(fun1), true);
assertEq(g_fun2.isSameNative(fun2), true);

// isSameNativeWithJitInfo can distinguish between fun1 and fun2.
assertEq(g_fun1.isSameNativeWithJitInfo(fun1), true);
assertEq(g_fun1.isSameNativeWithJitInfo(fun2), false);
assertEq(g_fun2.isSameNativeWithJitInfo(fun1), false);
assertEq(g_fun2.isSameNativeWithJitInfo(fun2), true);
