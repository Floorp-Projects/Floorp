// |jit-test| --no-sse4
if (!wasmCachingEnabled())
    quit(0);

var m = wasmCompileInSeparateProcess(wasmTextToBinary('(module (func (export "run") (result i32) (i32.const 42)))'));
assertEq(new WebAssembly.Instance(m).exports.run(), 42);
