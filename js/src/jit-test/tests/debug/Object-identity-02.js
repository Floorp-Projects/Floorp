// Different objects get different Debug.Object wrappers.
var g = newGlobal('new-compartment');
var dbg = Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame.arguments[0] === frame.arguments[1], false);
        hits++;
    }
};
g.eval("function f(a, b) { debugger; } f({}, {});");
assertEq(hits, 1);
