// arguments works in evalWithBindings (it does not interpose a function scope)
var g = newGlobal();
var dbg = new Debugger;
var global = dbg.addDebuggee(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var argc = frame.arguments.length;
    assertEq(argc, 8);
    assertEq(frame.evalWithBindings("arguments[prop]", {prop: "length"}).return, argc);
    for (var i = 0; i < argc; i++)
        assertEq(frame.evalWithBindings("arguments[i]", {i: i}).return, frame.arguments[i]);
    hits++;
};
g.eval("function f() { debugger; }");
var symbolExpr = typeof Symbol === "function" ? "Symbol('alpha')" : "'alpha'";
g.eval("f(undefined, -0, NaN, '\uffff', " + symbolExpr + ", Array.prototype, Math, f);");
assertEq(hits, 1);
