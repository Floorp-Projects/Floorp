// Setting a breakpoint in a generator function works, and we can
// traverse the stack and evaluate expressions in the context of older
// generator frames.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
    function hit(frame) {
        assertEq(frame.generator, true);
        assertEq(frame.older.generator, true);
        frame.older.eval("q += 16");
    }

    var s = frame.script;
    var offs = s.getLineOffsets(g.line0 + 9);
    for (var i = 0; i < offs.length; i++)
        s.setBreakpoint(offs[i], {hit: hit});
};

g.eval("line0 = Error().lineNumber;\n" +
       "function* g(x) {\n" +   // + 1
       "    var q = 10;\n" +    // + 2
       "    yield* x;\n" +      // + 3
       "    return q;\n" +      // + 4
       "}\n" +                  // + 5
       "function* range(n) {\n" + // + 6
       "    debugger;\n" +      // + 7
       "    for (var i = 0; i < n; i++)\n" + // + 8
       "        yield i;\n" +   // + 9  <-- breakpoint
       "    return;\n" + // so that line 9 only has the yield
       "}");

g.eval("var iter = g(range(2))");
g.eval("var first = iter.next().value");
g.eval("var second = iter.next().value");
g.eval("var third = iter.next().value");

assertEq(g.first, 0);
assertEq(g.second, 1);
assertEq(g.third, 42);
