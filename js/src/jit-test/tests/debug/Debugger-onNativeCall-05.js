// Test that the onNativeCall hook cannot return a primitive value.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

// Returning the callee accidentally is a common mistake when implementing C++
// methods, but the debugger should not trip any checks if it does this on
// purpose.
dbg.onNativeCall = (callee, reason) => {
    return {return: callee};
};
v = gdbg.executeInGlobal("new Object")
assertEq(v.return, gdbg.makeDebuggeeValue(g.Object));

// Returning a primitive should cause the hook to throw.
dbg.onNativeCall = (callee, reason) => {
    return {return: "primitive"};
};
v = gdbg.executeInGlobal("new Object")
assertEq(v.throw.proto, gdbg.makeDebuggeeValue(g.Error.prototype))

// A no-op hook shouldn't break any checks.
dbg.onNativeCall = (callee, reason) => { };
v = gdbg.executeInGlobal("new Object")
assertEq("return" in v, true);
