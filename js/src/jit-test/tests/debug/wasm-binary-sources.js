// Tests that wasm module scripts have access to binary sources.

load(libdir + "asserts.js");
load(libdir + "array-compare.js");

if (!wasmIsSupported())
  quit();

var g = newGlobal();
var dbg = new Debugger(g);

var s;
dbg.onNewScript = (script) => {
  s = script;
}

g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" 0))')));`);
assertEq(s.format, "wasm");

var source = s.source;

// The text is generated if wasm binary sources are disabled.
assertEq(source.text.includes('module'), true);
assertThrowsInstanceOf(() => source.binary, Error);

// Enable binary sources.
dbg.allowWasmBinarySource = true;

g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" 0))')));`);
assertEq(s.format, "wasm");

var source2 = s.source;

// The text is '[wasm]' if wasm binary sources are enabled.
assertEq(source2.text, '[wasm]');
// The binary contains Uint8Array which is equal to wasm bytecode;
arraysEqual(source2.binary, wasmTextToBinary('(module (func) (export "" 0))'));
