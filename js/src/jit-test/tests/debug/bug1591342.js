// |jit-test| error: Permission denied to access object

// jsfunfuzz-generated
newGlobal({ sameCompartmentAs: this });
nukeAllCCWs();
// Adapted from randomly chosen testcase: js/src/jit-test/tests/debug/clear-old-analyses-02.js
var g = newGlobal({
    newCompartment: true
});
var dbg = Debugger();
gw = dbg.addDebuggee(g);
g.eval("" + function fib() {});
gw.makeDebuggeeValue(g.fib).script.setBreakpoint(0, {});
