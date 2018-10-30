// |jit-test| error: ReferenceError
var g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () { hits++; };");
import('')();
