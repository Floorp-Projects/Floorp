// |jit-test| test-also-wasm-compiler-ion; skip-if: !wasmDebuggingIsSupported()

// Tests that wasm module scripts raises onEnterFrame and onLeaveFrame events.

load(libdir + "wasm.js");

// Checking if we stop at every wasm instruction during step.
var onEnterFrameCalled, onLeaveFrameCalled, onStepCalled;
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
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
      assertEq(onEnterFrameCalled, 1);
      assertEq(onLeaveFrameCalled, 1);
      assertEq(onStepCalled.length, 3);
      assertEq(onStepCalled[0] > 0, true);
      // The onStepCalled offsets are in ascending order.
      assertEq(onStepCalled[0] < onStepCalled[1], true);
      assertEq(onStepCalled[1] < onStepCalled[2], true);
  }
);

// Checking if step mode was disabled after debugger has been detached.
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({dbg, g}) {
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
            dbg.removeDebuggee(g);
        };
    },
    function ({error}) {
        assertEq(error, undefined);
        assertEq(onEnterFrameCalled, 1);
        assertEq(onLeaveFrameCalled, 0);
        assertEq(onStepCalled.length, 0);
    }
);
