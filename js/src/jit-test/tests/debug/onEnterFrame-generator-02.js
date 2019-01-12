// onEnterFrame fires after the [[GeneratorState]] is set to "executing".
//
// This test checks that Debugger doesn't accidentally make it possible to
// reenter a generator frame that's already on the stack. (Also tests a fun
// corner case in baseline debug-mode OSR.)

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval('function* f() { yield 1; yield 2; }');
let dbg = Debugger(g);
let genObj = null;
let hits = 0;
dbg.onEnterFrame = frame => {
    // The first time onEnterFrame fires, there is no generator object, so
    // there's nothing to test. The generator object doesn't exist until
    // JSOP_GENERATOR is reached, right before the initial yield.
    if (genObj !== null) {
        dbg.removeDebuggee(g);  // avoid the DebuggeeWouldRun exception
        assertThrowsInstanceOf(() => genObj.next(), g.TypeError);
        dbg.addDebuggee(g);
        hits++;
    }
};
genObj = g.f();
for (let x of genObj) {}
assertEq(hits, 3);
