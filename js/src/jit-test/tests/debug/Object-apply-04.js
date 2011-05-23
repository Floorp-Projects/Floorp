// |jit-test| debug
// Debug.Object.prototype.apply works with function proxies

var g = newGlobal('new-compartment');
g.eval("function f() { debugger; }");
var dbg = Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        var proxy = frame.arguments[0];
        assertEq(proxy.name, undefined);
        assertEq(proxy.apply(null, [33]).return, 34);
        hits++;
    }
};
g.eval("f(Proxy.createFunction({}, function (arg) { return arg + 1; }));");
assertEq(hits, 1);
