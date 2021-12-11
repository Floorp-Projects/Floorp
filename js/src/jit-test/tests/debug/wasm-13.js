// |jit-test| skip-if: !wasmDebuggingEnabled()
// Tests that wasm module scripts has inspectable globals and memory.

load(libdir + "wasm.js");
load(libdir + 'eqArrayHelper.js');

function monitorGlobalValues(wast, lib, expected) {
    function setupFrame(frame) {
        var globals = {};
        framesGlobals.push(globals);
        // Environment with globals follow function scope enviroment
        var globalEnv = frame.environment.parent;
        globalEnv.names().forEach(n => {
            globals[n] = [globalEnv.getVariable(n)];
        });
        frame.onStep = function () {
            var globalEnv = frame.environment.parent;
            globalEnv.names().forEach(n => {
                var prevValues = globals[n];
                if (!prevValues)
                    globals[n] = prevValues = [void 0];
                var value = globalEnv.getVariable(n);
                if (prevValues[prevValues.length - 1] !== value)
                    prevValues.push(value);
            });
        }
    }
    var framesGlobals = [];
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
    assertEq(framesGlobals.length, expected.length);
    for (var i = 0; i < framesGlobals.length; i++) {
        var frameGlobals = framesGlobals[i];
        var expectedGlobals = expected[i];
        var globalsNames = Object.keys(frameGlobals);
        assertEq(globalsNames.length, Object.keys(expectedGlobals).length);
        globalsNames.forEach(n => {
            if (typeof expectedGlobals[n][0] === "function") {
                // expectedGlobals are assert functions
                expectedGlobals[n].forEach((assertFn, i) => {
                    assertFn(frameGlobals[n][i]);
                });
                return;
            }
            assertEqArray(frameGlobals[n], expectedGlobals[n]);
        });
    }
}

monitorGlobalValues(
    '(module (func (export "test") (nop)))',
    undefined,
    [{}]
);
monitorGlobalValues(
    '(module (memory (export "memory") 1 1) (func (export "test") (nop) (nop)))',
    undefined,
    [{
        memory0: [
            function (actual) {
                var bufferProp = actual.proto.getOwnPropertyDescriptor("buffer");
                assertEq(!!bufferProp, true, "wasm memory buffer property");
                var buffer = bufferProp.get.call(actual).return;
                var bufferLengthProp = buffer.proto.getOwnPropertyDescriptor("byteLength");
                var bufferLength = bufferLengthProp.get.call(buffer).return;
                assertEq(bufferLength, 65536, "wasm memory size");
            }
        ]
    }]
);
monitorGlobalValues(
    `(module\
     (global i32 (i32.const 1))(global i64 (i64.const 2))(global f32 (f32.const 3.5))(global f64 (f64.const 42.25))\
     (global externref (ref.null extern))\
     (func (export "test") (nop)))`,
    undefined,
    [(function () {
        let x = {
            global0: [1],
            global1: [2],
            global2: [3.5],
            global3: [42.25],
            global4: [ function (x) { assertEq(x.optimizedOut, true); } ],
        };
        return x;
    })()]
);
monitorGlobalValues(
    `(module (global (mut i32) (i32.const 1))(global (mut i64) (i64.const 2))\
             (global (mut f32) (f32.const 3.5))(global (mut f64) (f64.const 42.25))\
             (global (mut externref) (ref.null extern))\
     (func (export "test")\
       (i32.const 2)(global.set 0)(i64.const 1)(global.set 1)\
       (f32.const 42.25)(global.set 2)(f64.const 3.5)(global.set 3)\
       (ref.null extern)(global.set 4)))`,
    undefined,
    [(function () {
        let x = {
            global0: [1, 2],
            global1: [2, 1],
            global2: [3.5, 42.25],
            global3: [42.25, 3.5],
            global4: [ function (x) { assertEq(x.optimizedOut, true); } ],
        };
        return x;
    })()]
)
