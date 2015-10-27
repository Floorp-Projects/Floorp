// setVariable works on let-bindings.

var g = newGlobal();
function test(code, val) {
    g.eval("function f() { " + code + " }");
    var dbg = new Debugger(g);
    dbg.onDebuggerStatement = function (frame) {
        frame.environment.setVariable("a", val);
    };
    assertEq(g.f(), val);
}

test("let a = 1; debugger; return a;", "xyzzy");
test("{ let a = 1; debugger; return a; }", "plugh");
