// |jit-test| error: InternalError
var g = newGlobal();
var dbg = new Debugger(g);
g.eval("function f(n) { if (n == 0) debugger; else f(n - 1); }");
g.f("function f() { debugger; }");
