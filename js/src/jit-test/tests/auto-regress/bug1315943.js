var s = "{}";
for (var i = 0; i < 21; i++) s += s;
var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function(frame) {
    var s = frame.eval("f").return.script;
};
g.eval("line0 = Error().lineNumber;\n" + "debugger;\n" + // line0 + 1
    "function f(i) {\n" + // line0 + 2
    s + // line0 + 3 ... line0 + where - 2
    "}\n");
