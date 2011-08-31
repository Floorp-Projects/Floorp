// var statements in strict evalWithBindings code behave like strict eval.
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.evalWithBindings("var i = a*a + b*b; i === 25;", {a: 3, b: 4}).return, true);
    hits++;
};
g.eval("'use strict'; debugger;");
assertEq(hits, 1);
assertEq("i" in g, false);

g.eval("function die() { throw fit; }");
g.eval("Object.defineProperty(this, 'i', {get: die, set: die});");
g.eval("'use strict'; debugger;");
assertEq(hits, 2);
