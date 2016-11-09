// Tests that JavaScript scripts have a "js" format and wasm scripts have a
// "wasm" format.

var g = newGlobal();
var dbg = new Debugger(g);

var gotScript;
dbg.onNewScript = (script) => {
  gotScript = script;
};

g.eval(`(() => {})()`);
assertEq(gotScript.format, "js");

if (!wasmIsSupported())
  quit();

g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" 0))')));`);
assertEq(gotScript.format, "wasm");
