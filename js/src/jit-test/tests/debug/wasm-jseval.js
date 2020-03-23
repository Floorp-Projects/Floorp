// |jit-test| skip-if: !wasmDebuggingIsSupported()
// Tests that JS can be evaluated on wasm module scripts frames.

load(libdir + "wasm.js");

wasmRunWithDebugger(
    '(module (memory 1 1)\
     (global (mut f64) (f64.const 0.5))\
     (global f32 (f32.const 3.5))\
     (func (param i32) (local f64) (f64.const 1.0) (tee_local 1) (set_global 0) (nop))\
     (export "test" (func 0))\
     (data (i32.const 0) "Abc\\x2A"))',
    undefined,
    function ({dbg}) {
        dbg.onEnterFrame = function (frame) {
            if (frame.type != 'wasmcall') return;

            var memoryContent = frame.eval('new DataView(memory0.buffer).getUint8(3)').return;
            assertEq(memoryContent, 42, 'valid memory content is expected (0x2A)');

            var global1AndParamSum = frame.eval('global1 + var0').return;
            assertEq(global1AndParamSum, 3.5);

            var stepNumber = 0;
            frame.onStep = function () {
                switch (stepNumber) {
                  case 1: // after i64.const 1.0
                    assertEq(frame.eval('global0').return, 0.5);
                    assertEq(frame.eval('var1').return, 0.0);
                    break;
                  case 2: // after tee_local $var1
                    assertEq(frame.eval('var1').return, 1.0);
                    break;
                  case 3: // after set_global $global0
                    assertEq(frame.eval('global0').return, 1.0);
                    break;
                }
                stepNumber++;
            };
        };
  },
  function ({error}) {
      assertEq(error, undefined);
  }
);
