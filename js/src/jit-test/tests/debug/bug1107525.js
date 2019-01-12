// |jit-test| error: InternalError
enableGeckoProfiling();
var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () { hits++; };");
function f() {
    var x = f();
}
f();
