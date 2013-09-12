// Hitting a breakpoint with no hit method does nothing.

var g = newGlobal();
g.s = '';
g.eval("var line0 = Error().lineNumber;\n" +
       "function f() {\n" +   // line0 + 1
       "    debugger;\n" +  // line0 + 2
       "    s += 'x';\n" +  // line0 + 3
       "}\n")
var dbg = Debugger(g);
var bp = [];
dbg.onDebuggerStatement = function (frame) {
    g.s += 'D';
    var arr = frame.script.getLineOffsets(g.line0 + 3);
    for (var i = 0; i < arr.length; i++) {
        var obj = {};
        bp[i] = obj;
        frame.script.setBreakpoint(arr[i], obj);
    }
};

g.f();
assertEq(g.s, "Dx");

dbg.onDebuggerStatement = undefined;

for (var i = 0; i < bp.length; i++)
    bp[i].hit = function () { g.s += 'B'; };
g.f();
assertEq(g.s, "DxBx");
