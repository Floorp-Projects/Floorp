// Tests that function imports and function exports descriptors have
// signatures, in the test mode only, for fuzzers.

var module = new WebAssembly.Module(wasmTextToBinary(`(module
  (import $vv "env" "v_v")
  (export "v_v" $vv)

  (import $vi "env" "v_i" (param i32))
  (export "v_i" $vi)

  (import $vI "env" "v_I" (param i64))
  (export "v_I" $vI)

  (import $vf "env" "v_f" (param f32))
  (export "v_f" $vf)

  (import $mem "env" "memory" (memory 0))
  (export "mem" (memory $mem))

  (import $vd "env" "v_d" (param f64))
  (export "v_d" $vd)

  (import $vfd "env" "v_fd" (param f32) (param f64))
  (export "v_fd" $vfd)

  (import $vIfifd "env" "v_Ififd" (param i64) (param f32) (param i32) (param f32) (param f64))
  (export "v_Ififd" $vIfifd)

  (import $iv "env" "i_v" (result i32))
  (export "i_v" $iv)

  (import $Ii "env" "I_i" (result i64) (param i32))
  (export "I_i" $Ii)

  (import $table "env" "table" (table 0 anyfunc))
  (export "table" (table $table))

  (import $fd "env" "f_d" (result f32) (param f64))
  (export "f_d" $fd)

  (import $dffd "env" "d_ffd" (result f64) (param f32) (param f32) (param f64))
  (export "d_ffd" $dffd)
)`));

for (let desc of WebAssembly.Module.imports(module)) {
    assertEq(typeof desc.signature, 'undefined');
}
for (let desc of WebAssembly.Module.exports(module)) {
    assertEq(typeof desc.signature, 'undefined');
}

setJitCompilerOption('wasm.test-mode', 1);

function shortToType(s) {
    switch (s) {
        case 'v': return "";
        case 'i': return "i32";
        case 'I': return "i64";
        case 'f': return "f32";
        case 'd': return "f64";
    }
    throw new Error("unexpected shorthand type");
}

function expectedSignature(name) {
    let [ret, args] = name.split('_');

    args = args.split('').map(shortToType).join(', ');
    ret = shortToType(ret);

    return `(${args}) -> (${ret})`;
}

for (let desc of WebAssembly.Module.imports(module)) {
    if (desc.kind !== 'function') {
        assertEq(typeof desc.signature, 'undefined');
    } else {
        assertEq(desc.signature, expectedSignature(desc.name))
    }
}

for (let desc of WebAssembly.Module.exports(module)) {
    if (desc.kind !== 'function') {
        assertEq(typeof desc.signature, 'undefined');
    } else {
        assertEq(desc.signature, expectedSignature(desc.name))
    }
}
