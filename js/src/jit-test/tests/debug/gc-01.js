// Debuggers with enabled hooks should not be GC'd even if they are otherwise
// unreachable.

var g = newGlobal('new-compartment');
var actual = 0;
var expected = 0;

function f() {
    for (var i = 0; i < 20; i++) {
        var dbg = new Debugger(g);
        dbg.num = i;
        dbg.onDebuggerStatement = function (stack) { actual += this.num; };
        expected += i;
    }
}

f();
gc(); gc(); gc();
g.eval("debugger;");
assertEq(actual, expected);
