var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onNewScript = function (s) {
    throw new Error();
};
dbg.uncaughtExceptionHook = function () {}
g.eval("2 * 3");
