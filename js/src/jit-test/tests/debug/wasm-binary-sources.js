// |jit-test| skip-if: !wasmDebuggingIsSupported()

// Tests that wasm module scripts have access to binary sources.

load(libdir + "asserts.js");
load(libdir + "array-compare.js");

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

var s;
dbg.onNewScript = (script) => {
  s = script;
}

g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" (func 0)))')));`);
assertEq(s.format, "wasm");

var source = s.source;

// The text is never generated with the native Debugger API.
assertEq(source.text.includes('module'), false);

g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" (func 0)))')));`);
assertEq(s.format, "wasm");

var source2 = s.source;

// The text is predefined if wasm binary sources are enabled.
assertEq(source2.text, '[debugger missing wasm binary-to-text conversion]');
// The binary contains Uint8Array which is equal to wasm bytecode;
arraysEqual(source2.binary, wasmTextToBinary('(module (func) (export "" (func 0)))'));
