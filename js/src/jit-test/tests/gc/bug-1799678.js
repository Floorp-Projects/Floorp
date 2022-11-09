gczeal(0);
setMarkStackLimit(1);
loadFile(`
  function wasmEvalText(str, imports) {
    let binary = wasmTextToBinary(str);
    m = new WebAssembly.Module(binary);
    return new WebAssembly.Instance(m, imports);
  }
  let WasmFuncrefValues = [
    wasmEvalText(\`(module (func (export "")))\`).exports[''],
  ];
  g1 = newGlobal({newCompartment: true});
  gczeal(10,10);
`);
for (let i = 0; i < 1000; ++i)
  loadFile("}");
function loadFile(lfVarx) {
  try {
    evaluate(lfVarx);
  } catch (lfVare) {}
}
