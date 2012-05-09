// Test that 'arguments' access in a function that doesn't expect 'arguments'
// doesn't crash.

// TODO bug 659577: the debugger should be improved to throw an error in such
// cases rather than silently returning whatever it finds on the scope chain.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);

// capture arguments object and test function
dbg.onDebuggerStatement = function (frame) {
    args = frame.eval("arguments");
};
g.eval("function f() { debugger; }");
g.eval("f(this, f, {});");
