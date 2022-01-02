// |jit-test| error:ReferenceError

// for-of should close iterator even if the exception is once caught by the
// debugger.

var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () { };");
// jsfunfuzz-generated
for (var x of []) {};
for (var l of [0]) {
    for (var y = 0; y < 1; y++) {
        g2;
    }
}
