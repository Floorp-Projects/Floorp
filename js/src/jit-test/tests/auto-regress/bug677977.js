// Binary: cache/js-dbg-64-48e43edc8834-linux
// Flags:
//

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    var s = frame.eval("f").return.script;
};
function test(s) {
    g.eval("line0 = Error().lineNumber;\n" +
           "debugger;\n" +          // line0 + 1
           "function f(i) {\n" +    // line0 + 2
           "}\n");
}
test("i = 128;\n" +  "}\n");
var hits = 0;
dbg.onNewScript = function (s) {
    hits++;
};
assertEq(g.eval("eval('2 + 3')"), 5);
this.gczeal(hits, 2);
var fn = g.evaluate("(function (a) { return 5 + a; })", {compileAndGo: false});
var g2 = newGlobal();
dbg.addDebuggee(g2, dbg);
g2.clone(fn);
