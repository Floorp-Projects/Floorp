// |jit-test| --no-threads; --ion-eager
// Walking into Rematerialized frames under ordinary frames with their
// hasCachedSavedFrame bits set shouldn't cause an assertion.

enableTrackAllocations();
var g = newGlobal({ newCompartment: true });
var dbg = new Debugger;
g.toggle = function toggle(x, d) {
    if (d) {
        dbg.addDebuggee(g);
        var frame = dbg.getNewestFrame().older;
    }
};
g.eval("" + function f(x, d) {
    g(x, d);
});
g.eval("" + function g(x, d) {
    toggle(x, d);
});
g.eval("(" + function test() {
    for (var i = 0; i < 5; i++) f(42, false);
    f(42, true);
} + ")();");
