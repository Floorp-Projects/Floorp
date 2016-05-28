// |jit-test| allow-oom; --fuzzing-safe
// Adapted from randomly chosen test: js/src/jit-test/tests/modules/bug-1233915.js
var i = 100;
g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) frame.eval("");
} + ")()");
// Adapted from randomly chosen test: js/src/jit-test/tests/profiler/bug1242840.js
oomTest(function() {
    if (--i < 0)
        return;
    try {
        for (x of y);
    } catch (e) {
        x
    }
})
