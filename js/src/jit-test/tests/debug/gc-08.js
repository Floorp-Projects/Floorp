// |jit-test| debug
// Debuggers with enabled throw hooks should not be GC'd even if they are
// otherwise unreachable.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var actual = 0;
var expected = 0;

function f() {
    for (var i = 0; i < 20; i++) {
        var dbg = new Debug(g);
        dbg.hooks = {num: i, throw: function (stack, exc) { actual += this.num; }};
        expected += i;
    }
}

f();
gc();
assertThrowsValue(function () { g.eval("throw 'fit';"); }, "fit");
assertEq(actual, expected);
