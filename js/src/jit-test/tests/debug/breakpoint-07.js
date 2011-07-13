// Code runs fine if do-nothing breakpoints are set on every line.

var g = newGlobal('new-compartment');
var src = ("var line0 = Error().lineNumber;\n" +
           "function gcd(a, b) {\n" +       // line0 + 1
           "    if (a > b)\n" +             // line0 + 2
           "            return gcd(b, a);\n" +  // line0 + 3
           "    var c = b % a;\n" +         // line0 + 4
           "    if (c === 0)\n" +           // line0 + 5
           "            return a;\n" +          // line0 + 6
           "    return gcd(c, a);\n" +      // line0 + 7
           "}\n");                          // line0 + 8
g.eval(src);

var dbg = Debugger(g);
var hits = 0 ;
dbg.onDebuggerStatement = function (frame) {
    var s = frame.eval("gcd").return.script;
    var offs;
    for (var lineno = g.line0 + 2; (offs = s.getLineOffsets(lineno)).length > 0; lineno++) {
        for (var i = 0; i < offs.length; i++)
            s.setBreakpoint(offs[i], {hit: function (f) { hits++; }});
    }
    assertEq(lineno > g.line0 + 7, true);
    assertEq(lineno <= g.line0 + 9, true);
};

g.eval("debugger;");
assertEq(g.gcd(31 * 7 * 5 * 3 * 2, 11 * 3 * 3 * 2), 6);
assertEq(hits >= 18, true);
