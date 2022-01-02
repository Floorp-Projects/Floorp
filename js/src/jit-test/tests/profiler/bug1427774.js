setJitCompilerOption("baseline.warmup.trigger", 0);
enableGeckoProfiling();
try {
    enableSingleStepProfiling();
} catch(e) {
    quit();
}
function removeAdd(dbg, g) {
    dbg.removeDebuggee(g);
}
function newGlobalDebuggerPair(toggleSeq) {
    var g = newGlobal({newCompartment: true});
    var dbg = new Debugger;
    dbg.addDebuggee(g);
    g.eval("" + function f() {return 100});
    return [g, dbg];
}
function testTrap(toggleSeq) {
    var [g, dbg] = newGlobalDebuggerPair(toggleSeq);
    dbg.onEnterFrame = function(f) {
        f.script.setBreakpoint(Symbol.iterator == (this) ^ (this), {
            hit: function() {
                toggleSeq(dbg, g);
            }
        });
    };
    assertEq(g.f(), 100);
}
testTrap(removeAdd);
