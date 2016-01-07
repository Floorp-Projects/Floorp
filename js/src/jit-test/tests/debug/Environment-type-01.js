// env.type is 'object' in global environments and with-blocks, and 'declarative' otherwise.

var g = newGlobal();
var dbg = Debugger(g);
function test(code, expected) {
    var actual = '';
    g.h = function () { actual += dbg.getNewestFrame().environment.type; }
    g.eval(code);
    assertEq(actual, expected);
}

test("h();", 'declarative');
test("(function (s) { eval(s); })('var v = h();')", 'declarative');
test("(function (s) { h(); })();", 'declarative');
test("{let x = 1, y = 2; h();}", 'declarative');
test("with({x: 1, y: 2}) h();", 'with');
test("(function (s) { with ({x: 1, y: 2}) h(); })();", 'with');
test("{ let x = 1; h(); }", 'declarative');
test("for (let x = 0; x < 1; x++) h();", 'declarative');
test("for (let x in h()) ;", 'declarative');
test("for (let x in {a:1}) h();", 'declarative');
test("try { throw new Error; } catch (x) { h(x) }", 'declarative');
test("'use strict'; eval('var z = 1; h();');", 'declarative');

dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.eval("h(), 2 + 2;").return, 4);
}
test("debugger;", 'declarative');
test("(function f() { debugger; })();", 'declarative');
