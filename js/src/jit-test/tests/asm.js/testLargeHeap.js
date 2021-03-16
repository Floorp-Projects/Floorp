// |jit-test| skip-if: !largeArrayBufferEnabled(); --large-arraybuffers

load(libdir + "asm.js");

// This is a validly structured length but it is too large for our asm.js
// implementation: for the sake of simplicity, we draw the line at 7fff_ffff (ie
// really at 7f00_0000 given constraints on the format of the address).
var buf = new ArrayBuffer(0x8000_0000);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[' + BUF_MIN + '] = -1 } return f'), this, null, buf);
