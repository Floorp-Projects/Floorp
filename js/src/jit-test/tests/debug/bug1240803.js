// |jit-test| allow-oom


if (!('oomAfterAllocations' in this))
  quit();

(function() {
    g = newGlobal()
    dbg = new Debugger
    g.toggle = function(d) {
        if (d) {
            dbg.addDebuggee(g);
            dbg.getNewestFrame();
            oomAfterAllocations(2);
            setBreakpoint;
        }
    }
    g.eval("" + function f(d) toggle(d))
    g.eval("(" + function() {
        f(false);
        f(true);
    } + ")()")
})();

