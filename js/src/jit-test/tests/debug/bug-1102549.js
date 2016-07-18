// |jit-test| error: log is not defined
var g = newGlobal();
var dbg = new Debugger(g);
dbg.onPromiseSettled = function (g) { log += 's'; throw "foopy"; };
g.settleFakePromise(g.makeFakePromise());
