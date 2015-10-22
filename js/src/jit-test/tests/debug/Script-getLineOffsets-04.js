// getLineOffsets works with instructions reachable only by breaking out of a loop or switch.

var g = newGlobal();
g.line0 = null;
var dbg = Debugger(g);
var where;
dbg.onDebuggerStatement = function (frame) {
    var s = frame.eval("f").return.script;
    var lineno = g.line0 + where;
    var offs = s.getLineOffsets(lineno);
    for (var i = 0; i < offs.length; i++) {
        assertEq(s.getOffsetLocation(offs[i]).lineNumber, lineno);
        s.setBreakpoint(offs[i], {hit: function () { g.log += 'B'; }});
    }
    g.log += 'A';
};

function test(s) {
    var count = (s.split(/\n/).length - 1); // number of newlines in s
    g.log = '';
    where = 3 + count + 1;
    g.eval("line0 = Error().lineNumber;\n" +
           "debugger;\n" +          // line0 + 1
           "function f(i) {\n" +    // line0 + 2
           s +                      // line0 + 3 ... line0 + where - 2
           "    log += '?';\n" +    // line0 + where - 1
           "    log += '!';\n" +    // line0 + where
           "}\n");
    g.f(0);
    assertEq(g.log, 'A?B!');
}

test("i = 128;\n" +
     "for (;;) {\n" +
     "    var x = i - 10;;\n" +
     "    if (x < 0)\n" +
     "        break;\n" +
     "    i >>= 2;\n" +
     "}\n");

test("while (true)\n" +
     "    if (++i === 2) break;\n");

test("do {\n" +
     "    if (++i === 2) break;\n" +
     "} while (true);\n");

test("switch (i) {\n" +
     "  case 2: return 7;\n" +
     "  case 1: return 8;\n" +
     "  case 0: break;\n" +
     "  default: return -i;\n" +
     "}\n");
