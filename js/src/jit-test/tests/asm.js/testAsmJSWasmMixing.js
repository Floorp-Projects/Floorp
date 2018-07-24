load(libdir + "asm.js");
load(libdir + "wasm.js");

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Memory = WebAssembly.Memory;

var asmJS = asmCompile('stdlib', 'ffis', 'buf', USE_ASM + 'var i32 = new stdlib.Int32Array(buf); return {}');

var asmJSBuf = new ArrayBuffer(BUF_MIN);
asmLink(asmJS, this, null, asmJSBuf);

var wasmMem = wasmEvalText('(module (memory 1 1) (export "mem" memory))').exports.mem;
assertAsmLinkFail(asmJS, this, null, wasmMem.buffer);

setJitCompilerOption('asmjs.atomics.enable', 1);

var sharedAsmJS = asmCompile('stdlib', 'ffis', 'buf',
			     USE_ASM +
			     'var i32 = new stdlib.Int32Array(buf);' +
			     'var aload = stdlib.Atomics.load;' + // Declare shared memory
			     'return {}');

// A growable memory cannot be used for asm.js (nor would you think that is a
// reasonable thing to do).

{
    let sharedWasmMem = new WebAssembly.Memory({initial:2, maximum:3, shared:true});
    assertAsmLinkFail(sharedAsmJS, this, null, sharedWasmMem.buffer);
}

// A fixed-length shared memory cannot be used for asm.js, even though you might
// think that ought to be possible.

{
    let sharedWasmMem = new WebAssembly.Memory({initial:2, maximum:2, shared:true});
    assertAsmLinkFail(sharedAsmJS, this, null, sharedWasmMem.buffer);
}
