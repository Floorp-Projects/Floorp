// |jit-test| --setpref=wasm_gc; include:wasm.js; include: wasm-binary.js;

// Limit of subtyping hierarchy 63 deep
function moduleSubtypingDepth(depth) {
  let types = [];
  types.push({final: false, kind: FuncCode, args: [], ret: []});
  for (let i = 1; i <= depth; i++) {
    types.push({final: false, sub: i - 1, kind: FuncCode, args: [], ret: []});
  }
  return moduleWithSections([typeSection(types)]);
}
wasmValidateBinary(moduleSubtypingDepth(63));
wasmFailValidateBinary(moduleSubtypingDepth(64), /too deep/);
