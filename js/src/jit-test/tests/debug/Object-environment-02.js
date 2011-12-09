// The .environment of a function Debugger.Object is an Environment object.

var g = newGlobal('new-compartment')
var dbg = Debugger(g);
var hits = 0;
g.h = function () {
    var frame = dbg.getNewestFrame();
    var fn = frame.eval("j").return;
    assertEq(fn.environment instanceof Debugger.Environment, true);
    var closure = frame.eval("f").return;
    assertEq(closure.environment instanceof Debugger.Environment, true);
    hits++;
};
g.eval("function j(a) {\n" +
       "    var f = function () { return a; };\n" +
       "    h();\n" +
       "    return f;\n" +
       "}\n" +
       "j(0);\n");
assertEq(hits, 1);
