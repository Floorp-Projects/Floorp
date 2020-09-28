// |jit-test| skip-if: !wasmDebuggingEnabled()

// Tests that wasm module scripts are available via onNewScript.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

var gotScript;
dbg.onNewScript = (script) => {
  gotScript = script;
}

g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" (func 0)))')));`);
assertEq(gotScript.format, "wasm");

var gotScript2 = gotScript;
g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "a" (func 0)))')));`);
assertEq(gotScript.format, "wasm");

// The two wasm Debugger.Scripts are distinct.
assertEq(gotScript !== gotScript2, true);
