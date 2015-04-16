// |jit-test| error: TypeError
var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("(" + function () {
        dbg = new Debugger(debuggeeGlobal);
        dbg.onExceptionUnwind = Map;
} + ")();");
throw new Error("oops");
