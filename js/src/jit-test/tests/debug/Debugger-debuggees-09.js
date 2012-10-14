// |jit-test| debug
// If hasDebuggee(x) is false, removeDebuggee(x) does nothing.

var dbg = new Debugger;

function check(obj) {
    // If obj is something we could never debug, hasDebuggee(obj) is false.
    assertEq(dbg.hasDebuggee(obj), false);

    // If hasDebuggee(x) is false, removeDebuggee(x) does nothing.
    assertEq(dbg.removeDebuggee(obj), undefined);
}

// global objects which happen not to be debuggees at the moment
var g1 = newGlobal('same-compartment');
check(g1);

// objects in a compartment that is already debugging us
var g2 = newGlobal('new-compartment');
g2.parent = this;
g2.eval("var dbg = new Debugger(parent);");
check(g2);
