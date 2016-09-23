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

if (!getBuildConfiguration().x64 && isSimdAvailable() && this["SIMD"]) {
    var simdJS = asmCompile('stdlib', 'ffis', 'buf', USE_ASM + 'var i32 = new stdlib.Int32Array(buf); var i32x4 = stdlib.SIMD.Int32x4; return {}');
    assertAsmLinkFail(simdJS, this, null, asmJSBuf);
    assertAsmLinkFail(simdJS, this, null, wasmMem.buffer);

    var simdJSBuf = new ArrayBuffer(BUF_MIN);
    asmLink(simdJS, this, null, simdJSBuf);
    asmLink(simdJS, this, null, simdJSBuf);  // multiple SIMD.js instantiations succeed
    assertAsmLinkFail(asmJS, this, null, simdJSBuf);  // but not asm.js
}
