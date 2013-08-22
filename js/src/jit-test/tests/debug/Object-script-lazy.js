// Asking for the script of a Debugger.Object whose referent is a lazy
// function should work.

// Note that putting a compartment in debug mode automatically de-lazifies
// all its functions (so that findScripts works); so the only way to get a
// Debugger.Object whose referent is a lazy function is to get one
// referring to a non-debuggee function. (I don't think we should be
// handing out non-debuggee scripts anyway.)

// Create functions f, g in a non-debuggee compartment.
var g1 = newGlobal();
g1.eval('function f() { return "from f"; }');
g1.eval('function g() { return "from g"; }');

// Create a debuggee compartment with CCWs referring to f and g.
var g2 = newGlobal();
var dbg = new Debugger;
var g2w = dbg.addDebuggee(g2);
g2.f = g1.f;
g2.g = g1.g;

// At this point, g1.f should still be a lazy function. Unwrapping a D.O
// referring to g2's CCW of f should yield a D.O referring to f directly.
// Asking for that second D.O's script should de-lazify f.
var fDO = g2w.getOwnPropertyDescriptor('f').value;
assertEq(fDO.global, g2w);
assertEq(fDO.unwrap().global === g2w, false);
assertEq(fDO.unwrap().script instanceof Debugger.Script, true);

// Similarly for g1.g, and asking for its parameter names.
var gDO = g2w.getOwnPropertyDescriptor('g').value;
assertEq(gDO.global, g2w);
assertEq(gDO.unwrap().global === g2w, false);
assertEq(gDO.unwrap().parameterNames instanceof Array, true);
