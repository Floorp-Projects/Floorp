// |jit-test| skip-if: fuzzingSafe()

// Tests that function imports and function exports descriptors have
// signatures, in the test mode only, for fuzzers.

var module = new WebAssembly.Module(wasmTextToBinary(`(module
  (import "env" "v_v" (func $vv))
  (export "v_v" (func $vv))

  (import "env" "v_i" (func $vi (param i32)))
  (export "v_i" (func $vi))

  (import "env" "v_I" (func $vI (param i64)))
  (export "v_I" (func $vI))

  (import "env" "v_f" (func $vf (param f32)))
  (export "v_f" (func $vf))

  (import "env" "memory" (memory $mem 0))
  (export "mem" (memory $mem))

  (import "env" "v_d" (func $vd (param f64)))
  (export "v_d" (func $vd))

  (import "env" "v_fd" (func $vfd (param f32) (param f64)))
  (export "v_fd" (func $vfd))

  (import "env" "v_Ififd" (func $vIfifd (param i64) (param f32) (param i32) (param f32) (param f64)))
  (export "v_Ififd" (func $vIfifd))

  (import "env" "i_v" (func $iv (result i32)))
  (export "i_v" (func $iv))

  (import "env" "I_i" (func $Ii (param i32) (result i64)))
  (export "I_i" (func $Ii))

  (import "env" "table" (table $table 0 funcref))
  (export "table" (table $table))

  (import "env" "f_d" (func $fd (param f64) (result f32)))
  (export "f_d" (func $fd))

  (import "env" "d_ffd" (func $dffd (param f32) (param f32) (param f64) (result f64)))
  (export "d_ffd" (func $dffd))
)`));

for (let desc of WebAssembly.Module.imports(module)) {
    assertEq(typeof desc.signature, 'undefined');
}
for (let desc of WebAssembly.Module.exports(module)) {
    assertEq(typeof desc.signature, 'undefined');
}
