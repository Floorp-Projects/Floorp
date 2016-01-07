// Basic getChildScripts tests.

var g = newGlobal();
var dbg = new Debugger(g);
var log;
function note(s) {
    assertEq(s instanceof Debugger.Script, true);
    log += 'S';
    var c = s.getChildScripts();
    if (c.length > 0) {
        log += '[';
        for (var i = 0; i < c.length; i++)
            note(c[i]);
        log += ']';
    }
}
dbg.onDebuggerStatement = function (frame) { note(frame.script); };

function test(code, expected) {
    log = '';
    g.eval(code);
    assertEq(log, expected);
}

test("debugger;",
     "S");
test("function f() {} debugger;",
     "S[S]");
test("function g() { function h() { function k() {} return k; } return h; } debugger;",
     "S[S[S[S]]]");
test("function q() {} function qq() {} debugger;",
     "S[SS]");
test("[0].map(function id(a) { return a; }); debugger;",
     "S[S]");
test("Function('return 2+2;')(); debugger;",
     "S");
test("var obj = {get x() { return 0; }, set x(v) {}}; debugger;",
     "S[SS]");
test("function r(n) { for (var i = 0; i < n; i++) yield i; } debugger;",
     "S[S]");
test("function* qux(n) { for (var i = 0; i < n; i++) yield i; } debugger;",
     "S[S]");
