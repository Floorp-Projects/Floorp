// onStep hooks on suspended Frames can keep Debuggers alive, even chaining them.

// The path through the heap we're building and testing here is:
//     gen0 (generator object) -> frame1 (suspended Frame with .onStep) -> dbg1 (Debugger object)
//       -> gen1 -> frame2 -> dbg2
// where everything after `gen1` is otherwise unreachable, and the edges
// `frame1 -> dbg1` and `frames2 -> dbg2` are due to the .onStep handlers, not
// strong refrences.
//
// There is no easy way to thread an event through this whole path; when we
// call gen0.next(), it will fire frame1.onStep(), but from there, making sure
// gen1.next() is called requires some minor heroics (see the WeakMap below).

var gen0;

var hits2 = 0;
var resuming2 = false;

function onStep2() {
    if (resuming2) {
        hits2++;
        resuming2 = false;
    }
}

function setup() {
    let g1 = newGlobal({newCompartment: true});
    g1.eval(`
        function* gf1() {
             debugger;
             yield 1;
             return 'done';
        }
    `);
    gen0 = g1.gf1();

    let g2 = newGlobal({newCompartment: true});
    g2.eval(`
        function* gf2() { debugger; yield 1; return 'done'; }

        var resuming1 = false;

        function makeOnStepHook1(dbg1) {
            // We use this WeakMap as a weak reference from frame1.onStep to dbg1.
            var weak = new WeakMap();
            weak.set(dbg1, {});
            return () => {
                if (resuming1) {
                    var dbg1Arr = nondeterministicGetWeakMapKeys(weak);
                    assertEq(dbg1Arr.length, 1);
                    dbg1Arr[0].gen1.next();
                    resuming1 = false;
                }
            };
        }

        function test(g1, gen0) {
            let dbg1 = Debugger(g1);
            dbg1.onDebuggerStatement = frame1 => {
                frame1.onStep = makeOnStepHook1(dbg1);
                dbg1.onDebuggerStatement = undefined;
            };
            gen0.next();  // run to yield point, creating frame1 and setting its onStep hook
            resuming1 = true;
            dbg1.gen1 = gf2();
            return dbg1.gen1;
        }
    `);

    let dbg2 = Debugger(g2);
    dbg2.onDebuggerStatement = frame2 => {
        frame2.onStep = onStep2;
        dbg2.onDebuggerStatement = undefined;
    };
    var gen1 = g2.test(g1, gen0);
    gen1.next();  // run to yield point, creating frame2 and setting its onStep hook
    resuming2 = true;
}

setup();
gc();
assertEq(hits2, 0);
gen0.next();  // fires frame1.onStep, which calls gen1.next(), which fires frame2.onStep
assertEq(hits2, 1);
