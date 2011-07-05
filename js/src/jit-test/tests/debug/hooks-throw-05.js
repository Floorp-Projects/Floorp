// |jit-test| debug
// hooks.throw returning undefined does not affect the thrown exception.

var g = newGlobal('new-compartment');
g.parent = this;
g.eval("new Debugger(parent).hooks = {throw: function () {}};");

var obj = new Error("oops");
try {
    throw obj;
} catch (exc) {
    assertEq(exc, obj);
}
