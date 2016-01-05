function addRemove() {
    dbg.addDebuggee(g);
    f = dbg.getNewestFrame().older;
}
function removeAdd() {
    dbg.addDebuggee(g);
    var f = dbg.getNewestFrame();
    while (f) {
        f = f.older;
    }
}
function testInterrupt() {
    g = newGlobal();
    dbg = new Debugger;
    g.eval("" + function f() {
        return g();
    });
    g.eval("" + function g() {
        return h();
    });
    g.eval("" + function h() {
        for (var i = 0; i < 100; i++) {
            interruptIf(5);
        }
    });
    setInterruptCallback(function() {
        toggleSeq();
        return true;
    });
    g.f();
}
toggleSeq = addRemove;
testInterrupt();
toggleSeq = removeAdd;
testInterrupt();

