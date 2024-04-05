// |jit-test| skip-if: !wasmIsSupported()

// Use a Wasm module to get the following stack frames:
//
//  .. => array sort trampoline => wasmfunc comparator (Wasm) => comparator (JS)

let binary = wasmTextToBinary(`
(module
  (import "" "comparator" (func $comparator (param i32) (param i32) (result i32)))
  (func $wasmfunc
    (export "wasmfunc")
    (param $x i32)
    (param $y i32)
    (result i32)
    (return (call $comparator (local.get $x) (local.get $y)))
  )
)`);
let mod = new WebAssembly.Module(binary);
let instance = new WebAssembly.Instance(mod, {"": {comparator}});

function comparator(x, y) {
  readGeckoProfilingStack();
  return y - x;
}

enableGeckoProfilingWithSlowAssertions();

for (let i = 0; i < 20; i++) {
  let arr = [3, 1, 2, -1, 0, 4];
  arr.sort(instance.exports.wasmfunc);
  assertEq(arr.toString(), "4,3,2,1,0,-1");
}
