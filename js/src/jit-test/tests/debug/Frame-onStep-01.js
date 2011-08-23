// Simple Debugger.Frame.prototype.onStep test.
// Test that onStep fires often enough to see all four values of a.

var g = newGlobal('new-compartment');
g.a = 0;
g.eval("function f() {\n" +
       "    a += 2;\n" +
       "    a += 2;\n" +
       "    a += 2;\n" +
       "    return a;\n" +
       "}\n");

var dbg = Debugger(g);
var seen = [0, 0, 0, 0, 0, 0, 0];
dbg.onEnterFrame = function (frame) {
    frame.onStep = function () {
        assertEq(arguments.length, 0);
        assertEq(this, frame);
        seen[g.a] = 1;
    };
}

g.f();
assertEq(seen.join(""), "1010101");
