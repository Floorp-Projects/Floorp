// |jit-test| error:ReferenceError: iter is not defined
var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () {};");
function* f1() {
    for (const x of iter) {
        yield x;
    }
}
function f2() {
    for (var i of [1, 2, 3]) {
        for (var j of [4, 5, 6]) {
            for (const k of f1()) {
                break;
            }
        }
    }
}
f2();
