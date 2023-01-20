// Nested compilation units (say, an eval with in an eval) should have the
// correct sources attributed to them.
let g = newGlobal({newCompartment: true});
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

// `new Function(${string})` generates source wrapped with `function anonymous(args) {${string}}`
text = "function anonymous(\n) {\n\n}";
g.eval("new Function('')");

text = "function anonymous(a,b\n) {\nc\n}";
g.eval("new Function('a', 'b', 'c')");

text = "function anonymous(d\n,e\n) {\nf\n}";
g.eval("new Function('d\\n', 'e', 'f')");

evaluate("", { global: g });
text = "2 * 3";
evaluate("2 * 3", { global: g });
assertEq(count, 12);
