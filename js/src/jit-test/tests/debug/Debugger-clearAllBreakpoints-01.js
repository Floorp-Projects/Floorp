// clearAllBreakpoints clears breakpoints for the current Debugger object only.

var g = newGlobal({newCompartment: true});

var hits = 0;
function attach(i) {
    var dbg = Debugger(g);
    var handler = {
        hit: function (frame) {
            hits++;
            dbg.clearAllBreakpoints();
        }
    };

    dbg.onDebuggerStatement = function (frame) {
        var s = frame.script;
        var offs = s.getLineOffsets(g.line0 + 3);
        for (var i = 0; i < offs.length; i++)
            s.setBreakpoint(offs[i], handler);
    };
}
for (var i = 0; i < 4; i++)
    attach(i);

g.eval("var line0 = Error().lineNumber;\n" +
       "debugger;\n" +                      // line0 + 1
       "for (var i = 0; i < 7; i++)\n" +    // line0 + 2
       "    Math.sin(0);\n");               // line0 + 3
assertEq(hits, 4);
