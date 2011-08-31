// |jit-test| debug
// onExceptionUnwind assigning to argv[1] does not affect the thrown exception.

var g = newGlobal('new-compartment');
g.parent = this;
g.eval("function f(frame, exc) { f2 = function () { return exc; }; exc = 123; }");
g.eval("new Debugger(parent).onExceptionUnwind = f;");

var obj = new Error("oops");
try {
    throw obj;
} catch (exc) {
    assertEq(exc, obj);
}
