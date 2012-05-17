// env.type is 'object' in global environments and with-blocks, and 'declarative' otherwise.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
function test(code, expected) {
    var actual = '';
    g.h = function () { actual += dbg.getNewestFrame().environment.type; }
    g.eval(code);
    assertEq(actual, expected);
}

test("h();", 'object');
test("(function (s) { eval(s); })('var v = h();')", 'declarative');
test("(function (s) { h(); })();", 'declarative');
test("{let x = 1, y = 2; h();}", 'declarative');
test("with({x: 1, y: 2}) h();", 'with');
test("(function (s) { with ({x: 1, y: 2}) h(); })();", 'with');
test("let (x = 1) { h(); }", 'declarative');
test("(let (x = 1) h());", 'declarative');
test("for (let x = 0; x < 1; x++) h();", 'declarative');
test("for (let x in h()) ;", 'object');
test("for (let x in {a:1}) h();", 'declarative');
test("try { throw new Error; } catch (x) { h(x) }", 'declarative');
test("'use strict'; eval('var z = 1; h();');", 'declarative');
test("for (var x in [h(m) for (m in [1])]) ;", 'declarative');
test("for (var x in (h(m) for (m in [1]))) ;", 'declarative');

// Since a generator-expression is effectively a function, the innermost scope
// is a function scope, and thus declarative. Thanks to an odd design decision,
// m is already in scope at the point of the call to h(). The answer here is
// not all that important, but we shouldn't crash.
test("for (var x in (0 for (m in h()))) ;", 'declarative');

dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.eval("h(), 2 + 2;").return, 4);
}
test("debugger;", 'object');
test("(function f() { debugger; })();", 'declarative');
