// |jit-test| debug
// Debug.Function basics

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame.arguments[0], frame.callee);
        assertEq(Object.getPrototypeOf(frame.arguments[0]), Debug.Function.prototype);
        assertEq(frame.arguments[0] instanceof Debug.Function, true);
        assertEq(frame.arguments[0] instanceof Debug.Object, true);
        assertEq(Object.getPrototypeOf(frame.arguments[1]), Debug.Object.prototype);
        assertEq(frame.arguments[1] instanceof Debug.Function, false);
        assertEq(frame.arguments[1] instanceof Debug.Object, true);
        hits++;
    }
};

g.eval("var obj = {}; function f(a, b) { debugger; } f(f, obj);");
assertEq(hits, 1);
