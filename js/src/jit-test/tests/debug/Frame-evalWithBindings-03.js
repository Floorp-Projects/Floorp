// arguments works in evalWithBindings (it does not interpose a function scope)
// when the function expects 'arguments'
var g = newGlobal('new-compartment');
var dbg = new Debugger;
var global = dbg.addDebuggee(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var argc = frame.arguments.length;
    assertEq(argc, 7);
    assertEq(frame.evalWithBindings("arguments[prop]", {prop: "length"}).return, argc);
    for (var i = 0; i < argc; i++)
        assertEq(frame.evalWithBindings("arguments[i]", {i: i}).return, frame.arguments[i]);
    hits++;
};
g.eval("function f() { eval('arguments'); debugger; }");
g.eval("f(undefined, -0, NaN, '\uffff', Array.prototype, Math, f);");
assertEq(hits, 1);
