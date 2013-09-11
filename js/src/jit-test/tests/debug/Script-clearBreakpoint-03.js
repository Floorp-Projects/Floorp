// Clearing a breakpoint by handler can clear multiple breakpoints.

var g = newGlobal();
var dbg = Debugger(g);
var s;
dbg.onDebuggerStatement = function (frame) {
    s = frame.eval("f").return.script;
};
g.eval("var line0 = Error().lineNumber;\n" +
       "function f(a, b) {\n" +     // line0 + 1
       "    return a + b;\n" +      // line0 + 2
       "}\n" +
       "debugger;\n");

var hits = 0;
var handler = {hit: function (frame) { hits++; s.clearBreakpoint(handler); }};
var offs = s.getLineOffsets(g.line0 + 2);
for (var i = 0; i < 4; i++) {
    for (var j = 0; j < offs.length; j++)
        s.setBreakpoint(offs[j], handler);
}

assertEq(g.f(2, 2), 4);
assertEq(hits, 1);
assertEq(s.getBreakpoints().length, 0);
