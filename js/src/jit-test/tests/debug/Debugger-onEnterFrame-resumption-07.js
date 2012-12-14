// |jit-test| mjitalways
// Bug 745194.

var g = newGlobal('new-compartment');
g.eval("function f() {}" +
       "function h() { return new f; }");
var dbg = Debugger(g);
dbg.onEnterFrame = function (frame) {
    if (frame.constructing) {
        gc();
        return { return: 0 };
    }
};
var result = g.eval("h()");
assertEq(typeof result, 'object');
assertEq(Object.getPrototypeOf(result), g.f.prototype);
