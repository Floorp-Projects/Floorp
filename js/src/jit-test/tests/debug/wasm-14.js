// |jit-test| test-also=--wasm-compiler=optimizing; test-also=--wasm-function-references --wasm-gc; skip-if: !wasmDebuggingEnabled() || !wasmGcEnabled(); skip-if: true
// An extension of wasm-10.js, testing that wasm GC objects are inspectable in locals.

// As of bug 1825098, this test is disabled. (skip-if: true)
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1836320

load(libdir + "wasm.js");

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
        if (!prevValues) {
          locals[n] = prevValues = [void 0];
        }
        var value = frame.environment.getVariable(n);
        if (prevValues[prevValues.length - 1] !== value) {
          prevValues.push(value);
        }
      });
    }
  }
  var framesLocals = [];
  wasmRunWithDebugger(wast, lib,
    function ({dbg}) {
      dbg.onEnterFrame = function(frame) {
        if (frame.type == "wasmcall") {
          setupFrame(frame);
        }
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
    for (const n of localsNames) {
      const actualValues = frameLocals[n];
      const expectedValues = expectedLocals[n];
      for (let j = 0; j < expectedValues.length; j++) {
        const actual = actualValues[j];
        const expected = expectedValues[j];
        if (expected === null) {
          assertEq(actual, null, `values differed at index ${j}`);
        } else {
          for (const key of Object.keys(expected)) {
            const actualField = actual.getProperty(key).return;
            const expectedField = expected[key];
            assertEq(actualField, expectedField, `values differed for key "${key}"`);
          }
        }
      }
    }
  }
}

monitorLocalValues(
  `(module
  (type (struct i32 i64))
  (func (export "test")
    (local (ref null 0))
    (local.set 0 (struct.new 0 (i32.const 1) (i64.const 2)))
  )
)`,
  undefined,
  [{var0: [null, {0: 1, 1: 2n}]}]
);
monitorLocalValues(
  `(module
  (type (array i32))
  (func (export "test")
    (local (ref null 0))
    (i64.const 3)
    (local.set 0 (array.new 0 (i32.const 123) (i32.const 2)))
    drop
  )
)`,
  undefined,
  [{var0: [null, {length: 2, 0: 123, 1: 123}]}]
);
