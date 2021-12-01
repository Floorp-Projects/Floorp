// |jit-test| skip-if: !wasmGcEnabled()

function processWat(c) {
    binary = wasmTextToBinary(c)
    d = new WebAssembly.Module(binary)
    return new WebAssembly.Instance(d)
}
gczeal(14,10)
let { createA } = processWat(`
  (module (type $a (struct))
    (global $e (rtt $a) rtt.canon $a)
    (func
      (export "createA")
      (result eqref)
      global.get $e
      struct.new_with_rtt $a
    ))
`).exports;
createA();
