// |jit-test| --enable-arraybuffer-transfer; skip-if: !ArrayBuffer.prototype.transfer

let exp = wasmEvalText(`(module
    (memory 1)
    (export "mem" (memory 0))
    (func $f (result i32) (i32.load (i32.const 0)))
    (export "zero" (func $f))
)`).exports;

const byteLength = 65536;
let buffer = exp.mem.buffer;
let i32 = new Int32Array(buffer);
let zero = exp.zero;

const magic = 0xbadf00d;

assertEq(zero(), 0);
assertEq(i32[0], 0);

i32[0] = magic;

assertEq(zero(), magic);
assertEq(i32[0], magic);

assertEq(buffer.detached, false);
assertEq(buffer.byteLength, byteLength);

// Can't transfer Wasm prepared array buffers.
assertThrowsInstanceOf(() => buffer.transfer(), TypeError);

// |buffer| is still attached.
assertEq(buffer.detached, false);
assertEq(buffer.byteLength, byteLength);

// Access still returns the original value.
assertEq(zero(), magic);
assertEq(i32[0], magic);
