// |jit-test| error:7
var g = newGlobal({newCompartment: true})
g.parent = this
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {
        frame.older
    }
} + ")()")
function f1(i) {
    return f2(i|0);
};
function f2(i) {
    if (i === 0) throw 7;
    return f1(i - 1);
}
f1(10);
