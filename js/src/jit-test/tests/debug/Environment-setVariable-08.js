// setVariable throws if no binding exists.

load(libdir + "asserts.js");

function test(code) {
    var g = newGlobal('new-compartment');
    var dbg = new Debugger(g);
    var hits = 0;
    dbg.onDebuggerStatement = function (frame) {
        var env = frame.older.environment;
        assertThrowsInstanceOf(function () { env.setVariable("y", 2); }, Error);
        hits++;
    };
    g.eval("var y = 0; function d() { debugger; }");

    assertEq(g.eval(code), 0);

    assertEq(g.y, 0);
    assertEq(hits, 1);
}

// local scope of non-heavyweight function
test("function f() { var x = 1; d(); return y; }  f();");

// block scope
test("function h(x) { if (x) { let x = 1; d(); return y; } }  h(3);");

// strict eval scope
test("'use strict'; eval('d(); y;');");
