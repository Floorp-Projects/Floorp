// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

var code = `(module
  (import $i "env" "test")
  (func $t (call $i))
  (export "test" $t)
)`;
var mod = wasmEvalText(code, {
  env: {
    test: function() {
       // Expecting 3 lines in the backtrace (plus last empty).
       // The middle one is for the wasm function.
       var s = getBacktrace();
       assertEq(s.split('\n').length, 4);
       assertEq(s.split('\n')[1].startsWith("1 wasm-function[1]("), true);

       // Let's also run DumpBacktrace() to check if we are not crashing.
       backtrace();
    }
  }
}).exports;
mod.test();
