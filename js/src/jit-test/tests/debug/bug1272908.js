// |jit-test| error: out of memory; slow;

// Adapted from randomly chosen test: js/src/jit-test/tests/modules/bug-1233915.js
g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame)
    frame.eval("")
} + ")()");
// Adapted from randomly chosen test: js/src/jit-test/tests/debug/bug1254123.js
function ERROR(msg) {
    throw new Error("boom");
}
var dbg = new Debugger;
dbg.onNewGlobalObject = ERROR;
oomTest(function() {
    newGlobal();
})

