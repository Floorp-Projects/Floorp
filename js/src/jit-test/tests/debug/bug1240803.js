// |jit-test| allow-oom; skip-if: !hasFunction.oomAfterAllocations

(function() {
    g = newGlobal({newCompartment: true})
    dbg = new Debugger
    g.toggle = function(d) {
        if (d) {
            dbg.addDebuggee(g);
            dbg.getNewestFrame();
            oomAfterAllocations(2);
            setBreakpoint;
        }
    }
    g.eval("" + function f(d) { return toggle(d); })
    g.eval("(" + function() {
        f(false);
        f(true);
    } + ")()")
})();

