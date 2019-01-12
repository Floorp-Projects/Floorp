// |jit-test|
// Adding a debuggee must leave its scripts in a safe state.

var g = newGlobal({newCompartment: true});
g.eval(
    "function f(x) { return {q: x}; }\n" +
    "var n = f('').q;\n");
var dbg = new Debugger(g);
g.eval("f(0)");
