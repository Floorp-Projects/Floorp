// A breakpoint handler hit method can return {throw: exc} to throw an exception.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    function hit(frame) {
        return {throw: frame.eval("new Error('PASS')").return};
    }

    var s = frame.script;
    var offs = s.getLineOffsets(g.line0 + 3);
    for (var i = 0; i < offs.length; i++)
        s.setBreakpoint(offs[i], {hit: hit});
};

g.s = null;
g.eval("line0 = Error().lineNumber;\n" +
       "debugger;\n" +          // line0 + 1
       "try {\n" +              // line0 + 2
       "    s = 'FAIL';\n" +    // line0 + 3
       "} catch (exc) {\n" +
       "    assertEq(exc.constructor, Error);\n" +
       "    s = exc.message;\n" +
       "}\n");
assertEq(g.s, 'PASS');
