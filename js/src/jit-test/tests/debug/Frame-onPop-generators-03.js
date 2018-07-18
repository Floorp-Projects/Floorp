// onPop fires while the [[GeneratorState]] is still "executing".
//
// This test checks that Debugger doesn't accidentally make it possible to
// reenter a generator frame that's on the stack.

load(libdir + "asserts.js");

let g = newGlobal();
g.eval('function* f() { debugger; yield 1; debugger; yield 2; debugger; }');
let dbg = Debugger(g);
let genObj = g.f();

let hits = 0;
dbg.onDebuggerStatement = frame => {
    frame.onPop = completion => {
        dbg.removeDebuggee(g);  // avoid the DebuggeeWouldRun exception
        hits++;
        if (hits < 3) {
            // We're yielding. Calling .return(), .next(), or .throw() on a
            // generator that's currently on the stack fails with a TypeError.
            assertThrowsInstanceOf(() => genObj.next(), g.TypeError);
            assertThrowsInstanceOf(() => genObj.throw("fit"), g.TypeError);
            assertThrowsInstanceOf(() => genObj.return(), g.TypeError);
        } else {
            // This time we're returning. The generator has already been
            // closed, so its methods work but are basically no-ops.
            let result = genObj.next();
            assertEq(result.done, true);
            assertEq(result.value, undefined);

            assertThrowsValue(() => genObj.throw("fit"), "fit");

            result = genObj.return();
            assertEq(result.done, true);
            assertEq(result.value, undefined);
        }
        dbg.addDebuggee(g);
    };
};

for (let x of genObj) {}
assertEq(hits, 3);
