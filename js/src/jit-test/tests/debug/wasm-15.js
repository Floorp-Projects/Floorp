// |jit-test| test-also=--wasm-compiler=optimizing; skip-if: !wasmDebuggingEnabled() || !wasmTailCallsEnabled()

// Tests that wasm module scripts raises onEnterFrame and onLeaveFrame events in
// wasm return calls.

load(libdir + "wasm.js");

// Checking if enter/leave frame at return_call.
var onEnterFrameCalled, onLeaveFrameCalled, onStepCalled;
wasmRunWithDebugger(
    '(module (func) (func (return_call 0)) (func (call 1)) (export "test" (func 2)))',
    undefined,
    function ({dbg}) {
        onEnterFrameCalled = 0;
        onLeaveFrameCalled = 0;
        onStepCalled = [];
        dbg.onEnterFrame = function (frame) {
            if (frame.type != 'wasmcall') return;
            onEnterFrameCalled++;
            frame.onStep = function () {
                onStepCalled.push(frame.offset);
            };
            frame.onPop = function () {
                onLeaveFrameCalled++;
            };
        };
  },
  function ({error}) {
      assertEq(error, undefined);
      assertEq(onEnterFrameCalled, 3);
      assertEq(onLeaveFrameCalled, 3);
      assertEq(onStepCalled.length, 4);
      assertEq(onStepCalled[0] > 0, true);
  }
);

// Checking if enter/leave frame at return_call_indirect.
wasmRunWithDebugger(
    '(module (func) (func (return_call_indirect (i32.const 0))) (func (call 1)) (table 1 1 funcref) (elem (i32.const 0) 0) (export "test" (func 2)))',
    undefined,
    function ({dbg}) {
        onEnterFrameCalled = 0;
        onLeaveFrameCalled = 0;
        onStepCalled = [];
        dbg.onEnterFrame = function (frame) {
            if (frame.type != 'wasmcall') return;
            onEnterFrameCalled++;
            frame.onStep = function () {
                onStepCalled.push(frame.offset);
            };
            frame.onPop = function () {
                onLeaveFrameCalled++;
            };
        };
  },
  function ({error}) {
      assertEq(error, undefined);
      assertEq(onEnterFrameCalled, 3);
      assertEq(onLeaveFrameCalled, 3);
      assertEq(onStepCalled.length, 4);
      assertEq(onStepCalled[0] > 0, true);
  }
);

// Checking if enter/leave frame at return_call_ref.
wasmFunctionReferencesEnabled() && wasmRunWithDebugger(
    '(module (type $t (func)) (elem declare func 0) (func) (func (return_call_ref $t (ref.func 0))) (func (call 1)) (export "test" (func 2)))',
    undefined,
    function ({dbg}) {
        onEnterFrameCalled = 0;
        onLeaveFrameCalled = 0;
        onStepCalled = [];
        dbg.onEnterFrame = function (frame) {
            if (frame.type != 'wasmcall') return;
            onEnterFrameCalled++;
            frame.onStep = function () {
                onStepCalled.push(frame.offset);
            };
            frame.onPop = function () {
                onLeaveFrameCalled++;
            };
        };
  },
  function ({error}) {
      assertEq(error, undefined);
      assertEq(onEnterFrameCalled, 3);
      assertEq(onLeaveFrameCalled, 3);
      assertEq(onStepCalled.length, 5);
      assertEq(onStepCalled[0] > 0, true);
  }
);
