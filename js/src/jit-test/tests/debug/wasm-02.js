// Tests that wasm module scripts are available via onNewScript.

if (!wasmIsSupported())
  quit();

var g = newGlobal();
var dbg = new Debugger(g);

var gotScript;
dbg.onNewScript = (script) => {
  gotScript = script;
}

g.eval(`o = Wasm.instantiateModule(wasmTextToBinary('(module (func) (export "" 0))'));`);
assertEq(gotScript.format, "wasm");

var gotScript2 = gotScript;
g.eval(`o = Wasm.instantiateModule(wasmTextToBinary('(module (func) (export "a" 0))'));`);
assertEq(gotScript.format, "wasm");

// The two wasm Debugger.Scripts are distinct.
assertEq(gotScript !== gotScript2, true);
