// Test interaction between exception handler, debugger's onExceptionUnwind,
// and profiler's frame iterator.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

g.eval("" + function f() {
    for (var i = 0; i < 120; i++) {
        try { throw 1; } catch {}
    }
});

var count = 0;
dbg.onExceptionUnwind = function() {
    enableGeckoProfiling();
    readGeckoProfilingStack();
    disableGeckoProfiling();
    count++;
};

g.f();
assertEq(count, 120);
