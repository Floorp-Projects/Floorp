// A breakpoint handler hit method can return null to raise an uncatchable error.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    g.log += 'D';

    function hit(frame) {
        g.log += 'H';
        return null;
    }

    var f = frame.eval("f").return;
    var s = f.script;
    var offs = s.getLineOffsets(g.line0 + 3);
    for (var i = 0; i < offs.length; i++)
        s.setBreakpoint(offs[i], {hit: hit});

    var rv = f.call();
    assertEq(rv, null);
    g.log += 'X';
};

g.log = '';
g.eval("line0 = Error().lineNumber;\n" +
       "function f() {\n" +         // line0 + 1
       "    try {\n" +              // line0 + 2
       "        log += '3';\n" +    // line0 + 3
       "    } catch (exc) {\n" +
       "        log += '5';\n" +
       "    }\n" +
       "}\n" +
       "debugger;\n");
assertEq(g.log, 'DHX');
