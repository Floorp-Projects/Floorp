// Breakpoints are dropped from eval scripts when they finish executing.
// (The eval cache doesn't cache breakpoints.)

var g = newGlobal('new-compartment');

g.line0 = undefined;
g.eval("function f() {\n" +
       "    return eval(s);\n" +
       "}\n");
g.s = ("line0 = Error().lineNumber;\n" +
       "debugger;\n" +          // line0 + 1
       "result = 'ok';\n");     // line0 + 2

var dbg = Debugger(g);
var hits = 0, bphits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.type, 'eval');
    assertEq(frame.script.getBreakpoints().length, 0);
    var h = {hit: function (frame) { bphits++; }};
    var offs = frame.script.getLineOffsets(g.line0 + 2);
    for (var i = 0; i < offs.length; i++)
        frame.script.setBreakpoint(offs[i], h);
    hits++;
};

for (var i = 0; i < 3; i++) {
    assertEq(g.f(), 'ok');
    assertEq(g.result, 'ok');
}
assertEq(hits, 3);
assertEq(bphits, 3);
