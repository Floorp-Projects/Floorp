// |jit-test| error: SyntaxError;
g = newGlobal({newCompartment: true});
dbg = new Debugger;
setInterruptCallback(function () {
    dbg.addDebuggee(g);
    dbg.getNewestFrame();
    return true;
});
g.eval("" + function f() {
    for (var i = 0; i < 1; evaluate("class h { constructor() {} }")) {
        interruptIf(1);
    }
});
g.f();
