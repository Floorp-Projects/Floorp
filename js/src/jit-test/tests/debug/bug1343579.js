// |jit-test| test-also=--wasm-compiler=optimizing; skip-if: !wasmDebuggingEnabled()
// Checking if Debugger.Script.isInCatchScope return false for wasm.

load(libdir + "wasm.js");

var results;
wasmRunWithDebugger(
    '(module (memory 1) ' +
    '(func $func0 i32.const 1000000 i32.load drop) ' +
    '(func (export "test") call $func0))',
    undefined,
    function ({dbg, wasmScript}) {
        results = [];
        dbg.onExceptionUnwind = function (frame, value) {
            if (frame.type != 'wasmcall') return;
            var result = wasmScript.isInCatchScope(frame.offset);
            results.push(result);
        };
  },
  function ({error}) {
      assertEq(error !== undefined, true);
      assertEq(results.length, 2);
      assertEq(results[0], false);
      assertEq(results[1], false);
  }
);
