// |jit-test| test-also-no-wasm-baseline
// Tests that wasm module scripts handles basic breakpoint operations.

load(libdir + "wasm.js");

if (!wasmDebuggingIsSupported())
    quit();

// Checking if we can stop at specified breakpoint.
var onBreakpointCalled;
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({wasmScript}) {
        var breakpoints = wasmGetScriptBreakpoints(wasmScript);
        assertEq(breakpoints.length, 3);
        assertEq(breakpoints[0].offset > 0, true);
        // Checking if breakpoints offsets are in ascending order.
        assertEq(breakpoints[0].offset < breakpoints[1].offset, true);
        assertEq(breakpoints[1].offset < breakpoints[2].offset, true);
        onBreakpointCalled = 0;
        breakpoints.forEach(function (bp) {
            var offset = bp.offset;
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
        assertEq(onBreakpointCalled, 3);
    }
);

// Checking if we can remove breakpoint one by one.
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({wasmScript}) {
        var breakpoints = wasmGetScriptBreakpoints(wasmScript);
        onBreakpointCalled = 0;
        var handlers = [];
        breakpoints.forEach(function (bp, i) {
            var offset = bp.offset;
            wasmScript.setBreakpoint(offset, handlers[i] = {
                hit: (frame) => {
                    assertEq(frame.offset, breakpoints[0].offset);
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
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({wasmScript}) {
        var breakpoints = wasmGetScriptBreakpoints(wasmScript);
        onBreakpointCalled = 0;
        var handlers = [];
        breakpoints.forEach(function (bp, i) {
            var offset = bp.offset;
            wasmScript.setBreakpoint(offset, handlers[i] = {
                hit: (frame) => {
                    assertEq(frame.offset, breakpoints[0].offset);
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
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({dbg, wasmScript}) {
        var breakpoints = wasmGetScriptBreakpoints(wasmScript);
        onBreakpointCalled = 0;
        onStepCalled = [];
        var handlers = [];
        breakpoints.forEach(function (bp, i) {
            var offset = bp.offset;
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
        assertEq(onStepCalled.length, 3);
    }
);

// Checking if we can remove all breakpoints.
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({wasmScript}) {
        var breakpoints = wasmGetScriptBreakpoints(wasmScript);
        onBreakpointCalled = 0;
        breakpoints.forEach(function (bp, i) {
            var offset = bp.offset;
            wasmScript.setBreakpoint(offset, {
                hit: (frame) => {
                    assertEq(frame.offset, breakpoints[0].offset);
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
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({dbg, wasmScript, g}) {
        var breakpoints = wasmGetScriptBreakpoints(wasmScript);
        onBreakpointCalled = 0;
        breakpoints.forEach(function (bp, i) {
            var offset = bp.offset;
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
