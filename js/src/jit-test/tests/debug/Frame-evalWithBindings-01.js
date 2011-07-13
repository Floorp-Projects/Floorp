// evalWithBindings basics

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.evalWithBindings("x", {x: 2}).return, 2);
    assertEq(frame.evalWithBindings("x + y", {x: 2}).return, 5);
    hits++;
};

// in global code
g.y = 3;
g.eval("debugger;");

// in function code
g.y = "fail";
g.eval("function f(y) { debugger; }");
g.f(3);

// in direct eval code
g.eval("function f() { var y = 3; eval('debugger;'); }");
g.f();

// in strict eval code with var
g.eval("function f() { 'use strict'; eval('var y = 3; debugger;'); }");
g.f();

// in a with block
g.eval("with ({y: 3}) { debugger; }");

// shadowing
g.eval("let (x = 50, y = 3) { debugger; }");

assertEq(hits, 6);
