// |jit-test| allow-oom; skip-if: !hasFunction.oomAfterAllocations

var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("(" + function() {
    oomAfterAllocations(100);
    var dbg = Debugger(debuggeeGlobal);
    dbg.onEnterFrame = function(frame) {}
} + ")()");
