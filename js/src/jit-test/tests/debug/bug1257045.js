// |jit-test| skip-if: !wasmDebuggingIsSupported()

fullcompartmentchecks(true);
var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
dbg.onNewScript = (function(script) {
    s = script;
})
g.eval(`new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" (func 0)))')));`);
s.source;
