// |jit-test| skip-if: !wasmDebuggingEnabled()
var g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("Debugger(parent).onExceptionUnwind = function () {};");
lfModule = new WebAssembly.Module(wasmTextToBinary(`
(module
  (export "f" (func $func0))
  (func $func0 (result i32)
    i32.const -1
  )
)
`));
processModule(lfModule);
function processModule(module, jscode) {
    for (let i = 0; i < 2; ++i) {
        imports = {}
        instance = new WebAssembly.Instance(module, imports);
    }
}
