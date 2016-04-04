if (!wasmIsSupported())
  quit();

fullcompartmentchecks(true);
var g = newGlobal();
var dbg = new Debugger(g);
dbg.onNewScript = (function(script) {
    s = script;
})
g.eval(`Wasm.instantiateModule(wasmTextToBinary('(module (func) (export "" 0))'));`);
s.source;
