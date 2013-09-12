// setVariable triggering a setter doesn't crash or explode.
// It should throw WouldRunDebuggee, but that isn't implemented yet.

function test(code) {
    var g = newGlobal();
    g.eval("function d() { debugger; }");
    var dbg = Debugger(g);
    var hits = 0;
    dbg.onDebuggerStatement = function (frame) {
        var env = frame.environment.find("x");
        try {
            env.setVariable("x", 0);
        } catch (exc) {
        }
        hits++;
    };
    g.eval(code);
}

test("Object.defineProperty(this, 'x', {set: function (v) {}}); d();");
test("Object.defineProperty(Object.prototype, 'x', {set: function (v) {}}); d();");
test("with ({set x(v) {}}) eval(d());");
