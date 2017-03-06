// Nested compilation units (say, an eval with in an eval) should have the
// correct sources attributed to them.
let g = newGlobal();
let dbg = new Debugger(g);

var text;
var count = 0;
dbg.onNewScript = function (script) {
    ++count;
    if (count % 2 == 0)
        assertEq(script.source.text, text);
}

g.eval("eval('" + (text = "") + "')");
g.eval("eval('" + (text = "2 * 3") + "')");
g.eval("new Function('" + (text = "") + "')");
g.eval("new Function('" + (text = "2 * 3") + "')");
evaluate("", { global: g });
evaluate("2 * 3", { global: g });
assertEq(count, 10);
