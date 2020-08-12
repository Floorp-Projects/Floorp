
// Generated from simd_f32x4.wast and simd_f64x2.wast.  Generator
// script is attached to https://bugzilla.mozilla.org/show_bug.cgi?id=1647288.

// These predicates test for quiet NaNs only.

function isAnyNaN32(bits) {
    assertEq(bits & 0x7FC0_0000, 0x7FC0_0000);
}
function isCanonicalNaN32(bits) {
    assertEq(bits & 0x7FFF_FFFF, 0x7FC0_0000);
}
function isAnyNaN64(bits) {
    assertEq(bits & 0x7FF8_0000_0000_0000n, 0x7FF8_0000_0000_0000n);
}
function isCanonicalNaN64(bits) {
    assertEq(bits & 0x7FFF_FFFF_FFFF_FFFFn, 0x7FF8_0000_0000_0000n);
}
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.min (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan nan nan nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan nan nan nan) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN32(mem[0]);
isCanonicalNaN32(mem[1]);
isCanonicalNaN32(mem[2]);
isCanonicalNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan -nan -nan -nan)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan -nan -nan -nan) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x0p+0 0x0p+0 0x0p+0 0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x0p+0 -0x0p+0 -0x0p+0 -0x0p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p-149 0x1p-149 0x1p-149 0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p-149 -0x1p-149 -0x1p-149 -0x1p-149)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p-126 0x1p-126 0x1p-126 0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p-126 -0x1p-126 -0x1p-126 -0x1p-126)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p-1 0x1p-1 0x1p-1 0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p-1 -0x1p-1 -0x1p-1 -0x1p-1)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1p+0 0x1p+0 0x1p+0 0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1p+0 -0x1p+0 -0x1p+0 -0x1p+0)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2 0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2 -0x1.921fb6p+2)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127 0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127 -0x1.fffffep+127)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 inf inf inf inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 inf inf inf inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -inf -inf -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -inf -inf -inf -inf)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan nan nan nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 nan nan nan nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan -nan -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -nan -nan -nan -nan)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 nan:0x200000 nan:0x200000 nan:0x200000 nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)) (v128.store (i32.const 32) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000))) (func (export "runv") (v128.store (i32.const 0) (f32x4.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f32x4.max (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000) (v128.const f32x4 -nan:0x200000 -nan:0x200000 -nan:0x200000 -nan:0x200000)))))
`)));
var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN32(mem[0]);
isAnyNaN32(mem[1]);
isAnyNaN32(mem[2]);
isAnyNaN32(mem[3]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan nan) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan -nan) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.min (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.min (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan nan) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isCanonicalNaN64(mem[0]);
isCanonicalNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan -nan)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan -nan) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x0p+0 0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x0p+0 0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x0p+0 -0x0p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x0p+0 -0x0p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1074 0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p-1074 0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1074 -0x1p-1074))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p-1074 -0x1p-1074)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1022 0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p-1022 0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1022 -0x1p-1022))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p-1022 -0x1p-1022)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p-1 0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p-1 0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p-1 -0x1p-1))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p-1 -0x1p-1)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1p+0 0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1p+0 0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1p+0 -0x1p+0))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1p+0 -0x1p+0)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1.921fb54442d18p+2 0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1.921fb54442d18p+2 -0x1.921fb54442d18p+2)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 0x1.fffffffffffffp+1023 0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -0x1.fffffffffffffp+1023 -0x1.fffffffffffffp+1023)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 inf inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 inf inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -inf -inf))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -inf -inf)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 nan nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan -nan))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -nan -nan)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 nan:0x4000000000000 nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module (memory (export "mem") 1 1) (func (export "setup") (v128.store (i32.const 16) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)) (v128.store (i32.const 32) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000))) (func (export "runv") (v128.store (i32.const 0) (f64x2.max (v128.load (i32.const 16)) (v128.load (i32.const 32))))) (func (export "run") (v128.store (i32.const 0) (f64x2.max (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000) (v128.const f64x2 -nan:0x4000000000000 -nan:0x4000000000000)))))
`)));
var mem = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.run();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
ins.exports.setup(); ins.exports.runv();
isAnyNaN64(mem[0]);
isAnyNaN64(mem[1]);
