var g = newGlobal('new-compartment');
g.eval("function f(n) { for (var i = 0; i < n; i++) f(0); }");
g.f(RUNLOOP + 1);
