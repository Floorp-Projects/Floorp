oomAfterAllocations(1, 2);
var x = wasmTextToBinary('(module (func (export "run") (result i32) i32.const 42))');
WebAssembly.compileStreaming(x);
drainJobQueue();
