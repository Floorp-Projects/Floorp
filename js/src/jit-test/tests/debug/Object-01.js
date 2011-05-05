// |jit-test| debug
// Debug.Object basics

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame.arguments[0], frame.callee);
        assertEq(Object.getPrototypeOf(frame.arguments[0]), Debug.Object.prototype);
        assertEq(frame.arguments[0] instanceof Debug.Object, true);
        assertEq(frame.arguments[0] !== frame.arguments[1], true);
        assertEq(Object.getPrototypeOf(frame.arguments[1]), Debug.Object.prototype);
        assertEq(frame.arguments[1] instanceof Debug.Object, true);
        hits++;
    }
};

g.eval("var obj = {}; function f(a, b) { debugger; } f(f, obj);");
assertEq(hits, 1);
