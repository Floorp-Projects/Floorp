// A breakpoint handler hit method can return {return: val} to force the frame to return.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    g.log += 'D';

    function hit(frame) {
        g.log += 'H';
        return {return: 'ok'};
    }

    var f = frame.eval("f").return;
    var s = f.script;
    var offs = s.getLineOffsets(g.line0 + 2);
    for (var i = 0; i < offs.length; i++)
        s.setBreakpoint(offs[i], {hit: hit});

    var rv = f.call();
    assertEq(rv.return, 'ok');
    g.log += 'X';
};

g.log = '';
g.eval("line0 = Error().lineNumber;\n" +
       "function f() {\n" +     // line0 + 1
       "    log += '2';\n" +    // line0 + 2
       "}\n" +
       "debugger;\n");
assertEq(g.log, 'DHX');
