// Tests that function imports and function exports descriptors have
// signatures, in the test mode only, for fuzzers.

var module = new WebAssembly.Module(wasmTextToBinary(`(module
  (import $vv "env" "v_v")
  (export "v_v" (func $vv))

  (import $vi "env" "v_i" (param i32))
  (export "v_i" (func $vi))

  (import $vI "env" "v_I" (param i64))
  (export "v_I" (func $vI))

  (import $vf "env" "v_f" (param f32))
  (export "v_f" (func $vf))

  (import $mem "env" "memory" (memory 0))
  (export "mem" (memory $mem))

  (import $vd "env" "v_d" (param f64))
  (export "v_d" (func $vd))

  (import $vfd "env" "v_fd" (param f32) (param f64))
  (export "v_fd" (func $vfd))

  (import $vIfifd "env" "v_Ififd" (param i64) (param f32) (param i32) (param f32) (param f64))
  (export "v_Ififd" (func $vIfifd))

  (import $iv "env" "i_v" (result i32))
  (export "i_v" (func $iv))

  (import $Ii "env" "I_i" (result i64) (param i32))
  (export "I_i" (func $Ii))

  (import $table "env" "table" (table 0 funcref))
  (export "table" (table $table))

  (import $fd "env" "f_d" (result f32) (param f64))
  (export "f_d" (func $fd))

  (import $dffd "env" "d_ffd" (result f64) (param f32) (param f32) (param f64))
  (export "d_ffd" (func $dffd))
)`));

for (let desc of WebAssembly.Module.imports(module)) {
    assertEq(typeof desc.signature, 'undefined');
}
for (let desc of WebAssembly.Module.exports(module)) {
    assertEq(typeof desc.signature, 'undefined');
}
