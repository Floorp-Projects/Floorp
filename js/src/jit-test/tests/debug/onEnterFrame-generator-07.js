// Bug 1561935. Test interaction between debug traps for step mode and
// DebugAfterYield.

let g = newGlobal({newCompartment: true});
g.eval('function* f() { yield 1; yield 2; }');
let dbg = Debugger(g);
let genObj = null;
let hits = 0;
dbg.onEnterFrame = frame => {
    // The first time onEnterFrame fires, there is no generator object, so
    // there's nothing to test. The generator object doesn't exist until
    // JSOP_GENERATOR is reached, right before the initial yield.
    if (genObj === null) {
        // Trigger step mode.
        frame.onStep = function() {};
    } else {
        dbg.removeDebuggee(g);
        hits++;
    }
};
genObj = g.f();
for (let x of genObj) {}
assertEq(hits, 1);
