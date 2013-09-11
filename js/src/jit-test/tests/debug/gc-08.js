// Debuggers with enabled onExceptionUnwind hooks should not be GC'd even if
// they are otherwise unreachable.

load(libdir + "asserts.js");

var g = newGlobal();
var actual = 0;
var expected = 0;

function f() {
    for (var i = 0; i < 20; i++) {
        var dbg = new Debugger(g);
        dbg.num = i;
        dbg.onExceptionUnwind = function (stack, exc) { actual += this.num; };
        expected += i;
    }
}

f();
gc();
assertThrowsValue(function () { g.eval("throw 'fit';"); }, "fit");
assertEq(actual, expected);
