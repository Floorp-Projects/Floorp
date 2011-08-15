// Basic tests for Debugger.Object.prototype.class.
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var arr = frame.arguments;
    assertEq(arr[0].class, "Object");
    assertEq(arr[1].class, "Array");
    assertEq(arr[2].class, "Function");
    assertEq(arr[3].class, "Date");
    assertEq(arr[4].class, "Proxy");
    hits++;
};
g.eval("(function () { debugger; })(Object.prototype, [], eval, new Date, Proxy.create({}));");
assertEq(hits, 1);
