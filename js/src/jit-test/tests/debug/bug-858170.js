var g = newGlobal('new-compartment');
var dbg = Debugger(g);
dbg.onNewScript = function (s) {
    throw new Error();
};
dbg.uncaughtExceptionHook = function () {}
g.eval("2 * 3");
