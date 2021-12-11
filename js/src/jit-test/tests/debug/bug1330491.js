// |jit-test| test-also=--wasm-compiler=optimizing; error: TestComplete

if (!wasmDebuggingEnabled())
     throw "TestComplete";

(function createShortLivedDebugger() {
    var g = newGlobal({newCompartment: true});
    g.debuggeeGlobal = this;
    g.eval("(" + function () {
        dbg = new Debugger(debuggeeGlobal);
    } + ")();");
})();

let module = new WebAssembly.Module(wasmTextToBinary('(module (func))'));
new WebAssembly.Instance(module);

gcslice(1000000);

new WebAssembly.Instance(module);

throw "TestComplete";
