// |jit-test| skip-if: wasmMemoryControlEnabled()

const { validate } = WebAssembly;

const UNRECOGNIZED_OPCODE = /unrecognized opcode/;

let simpleTests = [
    "(module (func (memory.discard (i32.const 0) (i32.const 65536))))",
];

for (let src of simpleTests) {
    let bin = wasmTextToBinary(src);
    assertEq(validate(bin), false);
    wasmCompilationShouldFail(bin, UNRECOGNIZED_OPCODE);
}

const mem = new WebAssembly.Memory({ initial: 1 });
assertEq(mem.discard, undefined);
