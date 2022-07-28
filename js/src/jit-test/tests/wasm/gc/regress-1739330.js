// |jit-test| skip-if: !wasmGcEnabled()

try {
  gczeal(4);
  function a(b) {
    binary = wasmTextToBinary(b)
    c = new WebAssembly.Module(binary)
    return new WebAssembly.Instance(c)
  }
  d = [];
  let { newStruct } = a(`
    (type $e (struct))
      (func (export "newStruct")
        (result eqref)
        struct.new $e
      )
  `).exports
  d.push(newStruct());
  gczeal(14, 7);
  throw d;
} catch (d) {
  assertEq(d instanceof Array, true);
}
