// setVariable triggering a setter throws WouldRunDebuggee.

load(libdir + "asserts.js");

function test(code) {
    var g = newGlobal({newCompartment: true});
    g.eval("function d() { debugger; }");
    var dbg = Debugger(g);
    var hits = 0;
    dbg.onDebuggerStatement = function (frame) {
        var env = frame.older.environment.find("x");
        assertThrowsInstanceOf(
            () => env.setVariable("x", 0),
            Debugger.DebuggeeWouldRun);
        hits++;
    };
    g.eval(code);
    assertEq(hits, 1);
}

test("Object.defineProperty(this, 'x', {set: function (v) {}}); d();");
test("Object.defineProperty(Object.prototype, 'x', {set: function (v) {}}); d();");
test("with ({set x(v) {}}) eval(d());");
