function wasmEvalText(str, imports, options) {
  let binary = wasmTextToBinary(str);
  m = new WebAssembly.Module(binary, options);
  return new WebAssembly.Instance(m, imports);
}
let ins = wasmEvalText(`
  (module
    (func (export "fill0") (param $r externref))
  )
`);
for (let i53 = 0; i53 < 1000; i53++) {
  ins.exports.fill0("hello")
}
