load(libdir + "wasm.js");

const Module = WebAssembly.Module;

// Create cross-compartment wrappers to typed arrays and array buffers.
var g = newGlobal();
var code1 = g.eval("wasmTextToBinary('(module)', 'new-format')");
var code2 = g.eval("wasmTextToBinary('(module)', 'new-format').buffer");

// Should get unwrapped.
assertEq(new Module(code1) instanceof Module, true);
assertEq(new Module(code2) instanceof Module, true);
