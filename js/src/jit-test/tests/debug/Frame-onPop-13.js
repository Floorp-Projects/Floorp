// One can set onPop handlers on some frames but not others.
var g = newGlobal();
g.eval("function f(n) { if (n > 0) f(n-1); else debugger; }");
var dbg = new Debugger(g);
var log;

Debugger.Frame.prototype.nthOlder = function nthOlder(n) {
    var f = this;
    while (n-- > 0)
        f = f.older;
    return f;
};

dbg.onEnterFrame = function handleEnter(f) {
    log += "(" + f.arguments[0];
};

function makePopHandler(n) {
    return function handlePop(c) {
        log += ")" + this.arguments[0];
        assertEq(this.arguments[0], n);
    };
}

dbg.onDebuggerStatement = function handleDebugger(f) {
    // Set onPop handers on some frames, and leave others alone. Vary the
    // spacing.
    f.nthOlder(2).onPop = makePopHandler(2);
    f.nthOlder(3).onPop = makePopHandler(3);
    f.nthOlder(5).onPop = makePopHandler(5);
    f.nthOlder(8).onPop = makePopHandler(8);
    f.nthOlder(13).onPop = makePopHandler(13);
};

log = '';
g.f(20);
assertEq(log, "(20(19(18(17(16(15(14(13(12(11(10(9(8(7(6(5(4(3(2(1(0)2)3)5)8)13");
