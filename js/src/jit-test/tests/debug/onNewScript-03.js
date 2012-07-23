var g = newGlobal('new-compartment');
var dbg = Debugger(g);
dbg.onNewScript = function (s) {
    eval(longScript);
}
const longScript = "var x = 1;\n" + new Array(5000).join("x + ") + "x";
g.eval(longScript);
