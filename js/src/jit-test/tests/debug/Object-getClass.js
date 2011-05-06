// |jit-test| debug

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        var arr = frame.arguments;
        assertEq(arr[0].getClass(), "Object");
        assertEq(arr[1].getClass(), "Array");
        assertEq(arr[2].getClass(), "Function");
        assertEq(arr[3].getClass(), "Date");
        assertEq(arr[4].getClass(), "Proxy");
        hits++;
    }
};

g.eval("(function () { debugger; })(Object.prototype, [], eval, new Date, Proxy.create({}));");
assertEq(hits, 1);
