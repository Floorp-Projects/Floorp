// |jit-test| debug
// Random objects can be the argument to hasDebuggee and removeDebuggee.
// If hasDebuggee(x) is false, removeDebuggee(x) does nothing.

var dbg = new Debugger;

function check(obj) {
    // If obj is something we could never debug, hasDebuggee(obj) is false.
    assertEq(dbg.hasDebuggee(obj), false);

    // If hasDebuggee(x) is false, removeDebuggee(x) does nothing.
    assertEq(dbg.removeDebuggee(obj), undefined);
}

// objects in this compartment, which we can't debug
check(this);
check({});
var g1 = newGlobal('same-compartment');
check(g1);
check(g1.eval("({})"));

// objects in a compartment that is already debugging us
var g2 = newGlobal('new-compartment');
g2.parent = this;
g2.eval("var dbg = new Debugger(parent);");
check(g2);
check(g2.dbg);
