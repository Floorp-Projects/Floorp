var lfModule = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "global" "func" (result i32))
        (func (export "func_0") (result i32)
         call 0 ;; calls the import, which is func #0
        )
    )
`));

enableGeckoProfiling();
WasmHelpers.startProfiling();
setJitCompilerOption("ion.warmup.trigger", 20);

Object.prototype[3] = 3;

var imports = {
    global: {
        func: function() {}
    }
}

for (let i = 0; i < 100; ++i)  {
    for (dmod in imports)
        for (dname in imports[dmod])
            if (imports[dmod][dname] == undefined) {}
    instance = new WebAssembly.Instance(lfModule, imports);
    print(i, instance.exports.func_0());
}
