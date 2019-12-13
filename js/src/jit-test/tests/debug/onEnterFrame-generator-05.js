// When resuming a generator triggers one Debugger's onEnterFrame handler,
// all Debuggers' Debugger.Frames become usable at once.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval(`
    function* f() {
       yield 1;
    }
`);
let dbg1 = new Debugger(g);
let dbg2 = new Debugger(g);

let hits = 0;
let savedFrame1;
let savedFrame2;
dbg1.onEnterFrame = frame => {
    if (savedFrame1 === undefined) {
        savedFrame1 = frame;
        savedFrame2 = dbg2.getNewestFrame();
    } else {
        hits++;
        assertEq(savedFrame1, frame);
        for (let f of [savedFrame2, savedFrame1]) {
            assertEq(f.type, "call");
            assertEq(f.callee.name, "f");
        }
    }
};

let values = [...g.f()];
assertEq(hits, 2);
assertDeepEq(values, [1]);
