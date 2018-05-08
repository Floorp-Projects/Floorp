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
       var frames = s.split('\n');
       assertEq(frames.length, 4);
       assertEq(/> WebAssembly.Module":wasm-function\[1\]:0x/.test(frames[1]), true);

       // Let's also run DumpBacktrace() to check if we are not crashing.
       backtrace();
    }
  }
}).exports;
mod.test();
