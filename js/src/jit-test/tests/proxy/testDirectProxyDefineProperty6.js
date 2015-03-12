// Bug 1133094 - Proxy.[[DefineOwnProperty]]() should not throw when asked to
// define a configurable accessor property over an existing configurable data
// property on the target, even if the trap leaves the target unchanged.

var hits = 0;
var p = new Proxy({x: 1}, {
    defineProperty(t, k, desc) {
        // don't bother redefining the existing property t.x
        hits++;
        return true;
    }
});

assertEq(Object.defineProperty(p, "x", {get: function () {}}), p);
assertEq(hits, 1);
assertEq(p.x, 1);
