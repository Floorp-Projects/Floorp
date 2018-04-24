// |jit-test| --wasm-gc

var lfModule = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "global" "func" (result i32))
        (func (export "func_0") (result i32)
         call 0 ;; calls the import, which is func #0
        )
    )
`));
processModule(lfModule, `
  var dbg = new Debugger;
  dbg.memory.takeCensus()
`);
function processModule(module, jscode) {
    imports = {}
    for (let descriptor of WebAssembly.Module.imports(module)) {
        imports[descriptor.module] = {}
        imports[descriptor.module][descriptor.name] = new Function("x", "y", "z", jscode);
        instance = new WebAssembly.Instance(module, imports);
        for (let descriptor of WebAssembly.Module.exports(module)) {
            print(instance.exports[descriptor.name]())
        }
    }
}
