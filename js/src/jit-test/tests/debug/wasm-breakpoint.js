// |jit-test| test-also=--wasm-compiler=optimizing; skip-if: !wasmDebuggingEnabled()
// Tests that wasm module scripts handles basic breakpoint operations.

load(libdir + "wasm.js");

function runTest(wast, initFunc, doneFunc) {
    let g = newGlobal({newCompartment: true});
    let dbg = new Debugger(g);

    g.eval(`
var b = wasmTextToBinary('${wast}');
var m = new WebAssembly.Instance(new WebAssembly.Module(b));
`);

    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
    var breakpoints = wasmScript.getPossibleBreakpointOffsets();

    initFunc({
        dbg,
        wasmScript,
        g,
        breakpoints,
    });

    let result, error;
    try {
        result = g.eval("m.exports.test()");
    } catch (ex) {
        error = ex;
    }

    doneFunc({
        dbg,
        result,
        error,
        wasmScript,
        g
    });
}


var onBreakpointCalled;

// Checking if we can stop at specified breakpoint.
runTest(
    '(module (func (nop) (nop)) (export "test" (func 0)))',
    function ({wasmScript, breakpoints}) {
        print(`${JSON.stringify(breakpoints)}`);
        assertEq(breakpoints.length, 2);
        assertEq(breakpoints[0] > 0, true);
        // Checking if breakpoints offsets are in ascending order.
        assertEq(breakpoints[0] < breakpoints[1], true);
        onBreakpointCalled = 0;
        breakpoints.forEach(function (offset) {
            wasmScript.setBreakpoint(offset, {
                hit: (frame) => {
                    assertEq(frame.offset, offset);
                    onBreakpointCalled++;
                }
            });
        });
    },
    function ({dbg, error}) {
        assertEq(error, undefined);
        assertEq(onBreakpointCalled, 2);
    }
);

// Checking if we can remove breakpoint one by one.
runTest(
    '(module (func (nop) (nop)) (export "test" (func 0)))',
    function ({wasmScript, breakpoints}) {
        onBreakpointCalled = 0;
        var handlers = [];
        breakpoints.forEach(function (offset, i) {
            wasmScript.setBreakpoint(offset, handlers[i] = {
                hit: (frame) => {
                    assertEq(frame.offset, breakpoints[0]);
                    onBreakpointCalled++;
                    // Removing all handlers.
                    handlers.forEach(h => wasmScript.clearBreakpoint(h));
                }
            });
        });
    },
    function ({error}) {
        assertEq(error, undefined);
        assertEq(onBreakpointCalled, 1);
    }
);

// Checking if we can remove breakpoint one by one from a breakpoint handler.
runTest(
    '(module (func (nop) (nop)) (export "test" (func 0)))',
    function ({wasmScript, breakpoints}) {
        onBreakpointCalled = 0;
        var handlers = [];
        breakpoints.forEach(function (offset, i) {
            wasmScript.setBreakpoint(offset, handlers[i] = {
                hit: (frame) => {
                    assertEq(frame.offset, breakpoints[0]);
                    onBreakpointCalled++;
                    // Removing all handlers.
                    handlers.forEach(h => wasmScript.clearBreakpoint(h));
                }
            });
        });
    },
    function ({error}) {
        assertEq(error, undefined);
        assertEq(onBreakpointCalled, 1);
    }
);

// Checking if we can remove breakpoint one by one from onEnterFrame,
// but onStep will still work.
var onStepCalled;
runTest(
    '(module (func (nop) (nop)) (export "test" (func 0)))',
    function ({dbg, wasmScript, breakpoints}) {
        onBreakpointCalled = 0;
        onStepCalled = [];
        var handlers = [];
        breakpoints.forEach(function (offset, i) {
            wasmScript.setBreakpoint(offset, handlers[i] = {
                hit: (frame) => {
                    assertEq(false, true);
                    onBreakpointCalled++;
                }
            });
        });
        dbg.onEnterFrame = function (frame) {
            if (frame.type != 'wasmcall') return;
            frame.onStep = function () {
                onStepCalled.push(frame.offset);
            };

            // Removing all handlers.
            handlers.forEach(h => wasmScript.clearBreakpoint(h));
        };
    },
    function ({error}) {
        assertEq(error, undefined);
        assertEq(onBreakpointCalled, 0);
        assertEq(onStepCalled.length, 2);
    }
);

// Checking if we can remove all breakpoints.
runTest(
    '(module (func (nop) (nop)) (export "test" (func 0)))',
    function ({wasmScript, breakpoints}) {
        onBreakpointCalled = 0;
        breakpoints.forEach(function (offset, i) {
            wasmScript.setBreakpoint(offset, {
                hit: (frame) => {
                    assertEq(frame.offset, breakpoints[0]);
                    onBreakpointCalled++;
                    // Removing all handlers.
                    wasmScript.clearAllBreakpoints();
                }
            });
        });
    },
    function ({error}) {
        assertEq(error, undefined);
        assertEq(onBreakpointCalled, 1);
    }
);

// Checking if breakpoints are removed after debugger has been detached.
runTest(
    '(module (func (nop) (nop)) (export "test" (func 0)))',
    function ({dbg, wasmScript, g, breakpoints}) {
        onBreakpointCalled = 0;
        breakpoints.forEach(function (offset, i) {
            wasmScript.setBreakpoint(offset, {
                hit: (frame) => {
                    onBreakpointCalled++;
                }
            });
        });
        dbg.onEnterFrame = function (frame) {
            dbg.removeDebuggee(g);
        };
    },
    function ({error}) {
        assertEq(error, undefined);
        assertEq(onBreakpointCalled, 0);
    }
);
