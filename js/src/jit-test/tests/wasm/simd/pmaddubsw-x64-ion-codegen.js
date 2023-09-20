// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion"

// Testing _mm_maddubs_epi16 / vpmaddubsw behavoir for all platforms.
//
// Bug 1762413 adds specialization for emscripten's pattern to directly
// emit PMADDUBSW machine code.

const isX64 = getBuildConfiguration().x64 && !getBuildConfiguration().simulator;

// Simple test.
const simple = wasmTextToBinary(`(module 
  (memory (export "memory") 1 1)
  (func $_mm_maddubs_epi16 (export "t") (param v128 v128) (result v128)
    local.get 1
    i32.const 8
    i16x8.shl
    i32.const 8
    i16x8.shr_s
    local.get 0
    v128.const i32x4 0x00ff00ff 0x00ff00ff 0x00ff00ff 0x00ff00ff
    v128.and
    i16x8.mul
    local.get 1
    i32.const 8
    i16x8.shr_s
    local.get 0
    i32.const 8
    i16x8.shr_u
    i16x8.mul
    i16x8.add_sat_s)
  (func (export "run")
    i32.const 0
    v128.const i8x16 0 2 1 2 1  2  -1   1    255 255 255 255   0 0 255 255 
    v128.const i8x16 1 0 3 4 -3 -4 -128 127  127 127 -128 -128 0 0 -128 127 
    call $_mm_maddubs_epi16
    v128.store
  )
)`);
var ins = new WebAssembly.Instance(new WebAssembly.Module(simple));
ins.exports.run();
var mem16 = new Int16Array(ins.exports.memory.buffer, 0, 8);
assertSame(mem16, [0, 11, -11, -32513, 32767, -32768, 0, -255]);

if (hasDisassembler() && isX64) {
  assertEq(wasmDis(ins.exports.t, {tier:"ion", asString:true}).includes('pmaddubsw'), true);
}

if (hasDisassembler() && isX64) {
  // Two pmaddubsw has common operand, and code was optimized.
  const realWorldOutput = wasmTextToBinary(`(module
     (memory 1 1)
     (func (export "test")
         (local i32 i32 i32 i32 v128 v128 v128 v128 v128 v128)
         local.get 0
         local.get 1
         i32.add
         local.set 2
         local.get 0
         i32.const 16
         i32.add
         local.set 0
         local.get 3
         local.set 1
         loop
             local.get 5
             local.get 0
             v128.load
             local.tee 5
             i32.const 7
             i8x16.shr_s
             local.tee 8
             local.get 1
             v128.load offset=240
             local.get 5
             v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000
             i8x16.eq
             local.tee 7
             v128.andnot
             i8x16.add
             local.get 8
             v128.xor
             local.tee 4
             i32.const 8
             i16x8.shl
             i32.const 8
             i16x8.shr_s
             local.get 5
             i8x16.abs
             local.tee 5
             v128.const i32x4 0x00ff00ff 0x00ff00ff 0x00ff00ff 0x00ff00ff
             v128.and
             local.tee 9
             i16x8.mul
             local.get 4
             i32.const 8
             i16x8.shr_s
             local.get 5
             i32.const 8
             i16x8.shr_u
             local.tee 4
             i16x8.mul
             i16x8.add_sat_s
             i16x8.add_sat_s
             local.set 5
 
             local.get 6
             local.get 8
             local.get 1
             v128.load offset=224
             local.get 7
             v128.andnot
             i8x16.add
             local.get 8
             v128.xor
             local.tee 6
             i32.const 8
             i16x8.shl
             i32.const 8
             i16x8.shr_s
             local.get 9
             i16x8.mul
             local.get 6
             i32.const 8
             i16x8.shr_s
             local.get 4
             i16x8.mul
             i16x8.add_sat_s
             i16x8.add_sat_s
             local.set 6
 
             local.get 1
             i32.const 128
             i32.add
             local.set 1
             local.get 0
             i32.const 16
             i32.add
             local.tee 0
             local.get 2
             i32.ne
             br_if 0
         end
))`);
 
  var ins = new WebAssembly.Instance(new WebAssembly.Module(realWorldOutput));
  const output = wasmDis(ins.exports.test, {tier:"ion", asString:true}).replace(/^[0-9a-f]{8}  (?:[0-9a-f]{2} )+\n?\s+/gmi, "");
  // Find two pmaddubsw+paddsw.
  const re = /\bv?pmaddubsw[^\n]+\nv?paddsw /g;
  assertEq(re.exec(output) != null, true);
  assertEq(re.exec(output) != null, true);
  assertEq(re.exec(output) == null, true);
  // No leftover PMULL, PSLLW, or PSRAW.
  assertEq(/pmullw|psllw|psraw/.test(output), false);
}
