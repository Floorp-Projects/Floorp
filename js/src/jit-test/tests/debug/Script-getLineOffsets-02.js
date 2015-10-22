// getLineOffsets correctly places the various parts of a ForStatement.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    function handler(line) {
        return {hit: function (frame) { g.log += "" + line; }};
    }

    var s = frame.eval("f").return.script;
    for (var line = 2; line <= 6; line++) {
        var offs = s.getLineOffsets(g.line0 + line);
        var h = handler(line);
        for (var i = 0; i < offs.length; i++) {
            assertEq(s.getOffsetLocation(offs[i]).lineNumber, g.line0 + line);
            s.setBreakpoint(offs[i], h);
        }
    }
};

g.log = '';
g.eval("var line0 = Error().lineNumber;\n" +
       "function f(n) {\n" +        // line0 + 1
       "    for (var i = 0;\n" +    // line0 + 2
       "         i < n;\n" +        // line0 + 3
       "         i++)\n" +          // line0 + 4
       "        log += '.';\n" +    // line0 + 5
       "    log += '!';\n" +        // line0 + 6
       "}\n" +
       "debugger;\n");
assertEq(g.log, "");
g.f(3);
assertEq(g.log, "235.435.435.436!");
