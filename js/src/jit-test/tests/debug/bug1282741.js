
function removeAdd(dbg, g) {
    dbg.removeDebuggee(g);
    dbg.addDebuggee(g);
    switch (dbg.removeDebuggee(g)) {}
}
function newGlobalDebuggerPair(toggleSeq) {
    var g = newGlobal();
    var dbg = new Debugger;
    dbg.addDebuggee(g);
    g.eval("" + function f() {
        for (var i = 0; i < 100; i++) interruptIf(i == 95);
    });
    setInterruptCallback(function() {
        return true;
    });
    return [g, dbg];
}
function testEpilogue(toggleSeq) {
    var [g, dbg] = newGlobalDebuggerPair(toggleSeq);
    dbg.onEnterFrame = function(f) {
        f.onPop = function() {
            toggleSeq(dbg, g);
        }
    };
    g.f()
}
testEpilogue(removeAdd);
