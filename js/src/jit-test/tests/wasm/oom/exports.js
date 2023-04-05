// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
  let text = `(module
    (type (func (param i32) (result i32)))
  )`;
  let binary = wasmTextToBinary(text);
  let module = new WebAssembly.Module(binary);
  let obj = module.exports();
  assertEq(obj instanceof Object, true);
});
