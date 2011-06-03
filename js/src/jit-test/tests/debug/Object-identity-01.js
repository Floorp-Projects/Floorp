// Two references to the same object get the same Debug.Object wrapper.
var g = newGlobal('new-compartment');
var dbg = Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame.arguments[0], frame.arguments[1]);
        hits++;
    }
};
g.eval("var obj = {}; function f(a, b) { debugger; } f(obj, obj);");
assertEq(hits, 1);
