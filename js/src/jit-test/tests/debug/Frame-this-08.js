// Frame.this and evalInFrame in arrow function that uses 'this'.
var g = newGlobal();
g.eval("x = 4");
g.eval("var foo = function() { 'use strict'; return () => this; }; var arrow = foo.call(3);");
var dbg = new Debugger(g);
var hits = 0;
dbg.onEnterFrame = function (frame) {
    if (frame.type === "eval")
	return;
    hits++;
    assertEq(frame.type, "call");
    assertEq(frame.this, 3);
    assertEq(frame.eval("this + 1").return, 4);
};
g.eval("arrow();");
assertEq(hits, 1);
