// |jit-test| test-also=--wasm-compiler=optimizing; skip-if: !wasmDebuggingEnabled()
// Tests that wasm module scripts has inspectable locals.

load(libdir + "wasm.js");
load(libdir + 'eqArrayHelper.js');

function monitorLocalValues(wast, lib, expected) {
    function setupFrame(frame) {
        var locals = {};
        framesLocals.push(locals);
        frame.environment.names().forEach(n => {
            locals[n] = [frame.environment.getVariable(n)];
        });
        frame.onStep = function () {
            frame.environment.names().forEach(n => {
                var prevValues = locals[n];
                if (!prevValues)
                    locals[n] = prevValues = [void 0];
                var value = frame.environment.getVariable(n);
                if (prevValues[prevValues.length - 1] !== value)
                    prevValues.push(value);
            });
        }
    }
    var framesLocals = [];
    wasmRunWithDebugger(wast, lib,
        function ({dbg}) {
            dbg.onEnterFrame = function(frame) {
                if (frame.type == "wasmcall")
                    setupFrame(frame);
            }
        },
        function ({error}) {
            assertEq(error, undefined);
        }
    );
    assertEq(framesLocals.length, expected.length);
    for (var i = 0; i < framesLocals.length; i++) {
        var frameLocals = framesLocals[i];
        var expectedLocals = expected[i];
        var localsNames = Object.keys(frameLocals);
        assertEq(localsNames.length, Object.keys(expectedLocals).length);
        localsNames.forEach(n => {
            assertEqArray(frameLocals[n], expectedLocals[n]);
        });
    }
}

monitorLocalValues(
    '(module (func (nop) (nop)) (export "test" (func 0)))',
    undefined,
    [{}]
);
monitorLocalValues(
    '(module (func (export "test") (local i32) (i32.const 1) (local.set 0)))',
    undefined,
    [{var0: [0, 1]}]
);
monitorLocalValues(
    '(module (func (export "test") (local f32) (f32.const 1.5) (local.set 0)))',
    undefined,
    [{var0: [0, 1.5]}]
);
monitorLocalValues(
    '(module (func (export "test") (local f64) (f64.const 42.25) (local.set 0)))',
    undefined,
    [{var0: [0, 42.25]}]
);
monitorLocalValues(
    `(module
  (func (param i32) (result i32) (local.get 0) (i32.const 2) (i32.add))
  (func (param i32) (local i32) (local.get 0) (call 0) (local.set 1))
  (func (export "test") (i32.const 1) (call 1))
)`.replace(/\n/g, " "),
    undefined,
    [{}, {var0: [1], var1: [0, 3]}, {var0: [1]}]
);
