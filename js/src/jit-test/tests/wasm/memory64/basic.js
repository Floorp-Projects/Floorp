// Basic tests around creating and linking memories with i64 indices

const MaxMemory64Field = 0x1_0000_0000_0000; // pages
const MaxUint32 = 0xFFFF_FFFF;

// test the validity of different i64 memory types in validation, compilation,
// and the JS-API.
function memoryTypeModuleText(shared, initial, max) {
  return `(module
            (memory i64 ${initial} ${max !== undefined ? max : ''} ${shared ? `shared` : ''}))`;
}
function memoryTypeDescriptor(shared, initial, max) {
  return {
    // TODO: "index" is not yet part of the spec
    // https://github.com/WebAssembly/memory64/issues/24
    index: 'i64',
    initial,
    maximum: max,
    shared,
  };
}
function validMemoryType(shared, initial, max) {
  wasmValidateText(memoryTypeModuleText(shared, initial, max));
  wasmEvalText(memoryTypeModuleText(shared, initial, max));
  // TODO: JS-API cannot specify pages above UINT32_MAX
  // https://github.com/WebAssembly/memory64/issues/24
  if (max <= MaxUint32) {
    new WebAssembly.Memory(memoryTypeDescriptor(shared, initial, max));
  }
}
function invalidMemoryType(shared, initial, max, compileMessage, jsMessage) {
  wasmFailValidateText(memoryTypeModuleText(shared, initial, max), compileMessage);
  assertErrorMessage(() => wasmEvalText(memoryTypeModuleText(shared, initial, max)), WebAssembly.CompileError, compileMessage);
  // TODO: JS-API cannot specify pages above UINT32_MAX
  // https://github.com/WebAssembly/memory64/issues/24
  if (max === undefined || max <= MaxUint32) {
    assertErrorMessage(() => new WebAssembly.Memory(memoryTypeDescriptor(shared, initial, max)), Error, jsMessage);
  }
}

// valid to define a memory with i64
validMemoryType(false, 0);
// valid to define max with i64
validMemoryType(false, 0, 1);
// invalid for min to be greater than max with i64
invalidMemoryType(false, 2, 1, /minimum must not be greater than maximum/, /bad Memory maximum size/);
// valid to define shared memory with max with i64
validMemoryType(true, 1, 2);
// invalid to define shared memory without max with i64
invalidMemoryType(true, 1, undefined, /maximum length required for shared memory/, /maximum is not specified/);

// test the limits of memory64
validMemoryType(false, 0, MaxMemory64Field);
invalidMemoryType(false, 0, MaxMemory64Field + 1, /maximum memory size too big/, /bad Memory maximum/);
validMemoryType(true, 0, MaxMemory64Field);
invalidMemoryType(true, 0, MaxMemory64Field + 1, /maximum memory size too big/, /bad Memory maximum/);

// test that linking requires index types to be equal
function testLink(importedIndexType, importIndexType) {
  let imported = new WebAssembly.Memory({
    // TODO: "index" is not yet part of the spec
    // https://github.com/WebAssembly/memory64/issues/24
    index: importedIndexType,
    initial: 0,
  });
  let testModule =
      `(module
         (memory (import "" "imported") ${importIndexType} 0))`;
  if (importedIndexType === importIndexType) {
    wasmEvalText(testModule, {"": {imported}});
  } else {
    assertErrorMessage(() => wasmEvalText(testModule, {"": {imported}}), WebAssembly.LinkError, /index type/);
  }
}

var memTypes = [
    ['i64', 'i64'],
    ['i32', 'i32'],
    ['i64', 'i32'],
    ['i32', 'i64']];

for ( let [a,b] of memTypes ) {
    testLink(a, b);
}

// Active data segments use the index type for the init expression

for ( let [memType,exprType] of memTypes ) {
    assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory ${memType} 1)
  (data (${exprType}.const 0) "abcde"))`)), memType == exprType);
}

// Validate instructions using 32/64-bit pointers in 32/64-bit memories.
//
// Valid offsets are temporarily restricted to <= UINT32_MAX even for 64-bit
// memories, see implementation limit in readLinearMemoryAddress in WasmOpIter.h.

var validOffsets = {i32: ['', 'offset=0x10000000'],
                    i64: ['', 'offset=0x10000000']}

// Basic load/store
for (let [memType, ptrType] of memTypes ) {
    for (let offs of validOffsets[memType]) {
        assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory ${memType} 1)
  (func (param $p ${ptrType}) (param $i i32) (param $l i64) (param $f f32) (param $d f64)
    (drop (i32.add (i32.const 1) (i32.load8_s ${offs} (local.get $p))))
    (drop (i32.add (i32.const 1) (i32.load8_u ${offs} (local.get $p))))
    (drop (i32.add (i32.const 1) (i32.load16_s ${offs} (local.get $p))))
    (drop (i32.add (i32.const 1) (i32.load16_u ${offs} (local.get $p))))
    (drop (i32.add (i32.const 1) (i32.load ${offs} (local.get $p))))
    (i32.store8 ${offs} (local.get $p) (local.get $i))
    (i32.store16 ${offs} (local.get $p) (local.get $i))
    (i32.store ${offs} (local.get $p) (local.get $i))
    (drop (i64.add (i64.const 1) (i64.load8_s ${offs} (local.get $p))))
    (drop (i64.add (i64.const 1) (i64.load8_u ${offs} (local.get $p))))
    (drop (i64.add (i64.const 1) (i64.load16_s ${offs} (local.get $p))))
    (drop (i64.add (i64.const 1) (i64.load16_u ${offs} (local.get $p))))
    (drop (i64.add (i64.const 1) (i64.load32_s ${offs} (local.get $p))))
    (drop (i64.add (i64.const 1) (i64.load32_u ${offs} (local.get $p))))
    (drop (i64.add (i64.const 1) (i64.load ${offs} (local.get $p))))
    (i64.store8 ${offs} (local.get $p) (local.get $l))
    (i64.store16 ${offs} (local.get $p) (local.get $l))
    (i64.store32 ${offs} (local.get $p) (local.get $l))
    (i64.store ${offs} (local.get $p) (local.get $l))
    (drop (f32.add (f32.const 1) (f32.load ${offs} (local.get $p))))
    (f32.store ${offs} (local.get $p) (local.get $f))
    (drop (f64.add (f64.const 1) (f64.load ${offs} (local.get $p))))
    (f64.store ${offs} (local.get $p) (local.get $d))
))`)), memType == ptrType);
    }
}

// Bulk memory operations
for (let [memType, ptrType] of memTypes ) {
    assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory ${memType} 1)
  (data $seg passive "0123456789abcdef")
  (func (param $p ${ptrType})
    (drop (${ptrType}.add (${ptrType}.const 1) (memory.size)))
    (drop (${ptrType}.add (${ptrType}.const 1) (memory.grow (${ptrType}.const 1))))
    (memory.copy (local.get $p) (${ptrType}.const 0) (${ptrType}.const 628))
    (memory.fill (local.get $p) (i32.const 37) (${ptrType}.const 1024))
    (memory.init $seg (local.get $p) (i32.const 3) (i32.const 5))
))`)), memType == ptrType);
}

// SIMD
if (wasmSimdEnabled()) {
    for (let [memType, ptrType] of memTypes ) {
        for (let offs of validOffsets[memType]) {
            print(memType + " " + ptrType + " " + offs);
            assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory ${memType} 1)
  (func (param $p ${ptrType}) (param $v v128) (param $w v128)
    (drop (i8x16.add (local.get $w) (v128.load ${offs} (local.get $p))))
    (v128.store ${offs} (local.get $p) (local.get $v))
    (drop (i8x16.add (local.get $w) (v128.load8_splat ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load16_splat ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load32_splat ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load64_splat ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load32_zero ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load64_zero ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load8_lane ${offs} 1 (local.get $p) (local.get $v))))
    (drop (i8x16.add (local.get $w) (v128.load16_lane ${offs} 1 (local.get $p) (local.get $v))))
    (drop (i8x16.add (local.get $w) (v128.load32_lane ${offs} 1 (local.get $p) (local.get $v))))
    (drop (i8x16.add (local.get $w) (v128.load64_lane ${offs} 1 (local.get $p) (local.get $v))))
    (v128.store8_lane ${offs} 1 (local.get $p) (local.get $v))
    (v128.store16_lane ${offs} 1 (local.get $p) (local.get $v))
    (v128.store32_lane ${offs} 1 (local.get $p) (local.get $v))
    (v128.store64_lane ${offs} 1 (local.get $p) (local.get $v))
    (drop (i8x16.add (local.get $w) (v128.load8x8_s ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load8x8_u ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load16x4_s ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load16x4_u ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load32x2_s ${offs} (local.get $p))))
    (drop (i8x16.add (local.get $w) (v128.load32x2_u ${offs} (local.get $p))))
))`)), memType == ptrType);
        }
    }
}

// Threads
if (wasmThreadsEnabled()) {
    for (let [memType, ptrType] of memTypes ) {
        for (let offs of validOffsets[memType]) {
            assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory ${memType} 1 100 shared)
  (func (param $p ${ptrType}) (param $i i32) (param $l i64)
    (drop (i32.add (i32.const 1) (memory.atomic.wait32 ${offs} (local.get $p) (i32.const 0) (i64.const 37))))
    (drop (i32.add (i32.const 1) (memory.atomic.wait64 ${offs} (local.get $p) (i64.const 0) (i64.const 37))))
    (drop (i32.add (i32.const 1) (memory.atomic.notify ${offs} (local.get $p) (i32.const 1))))
))`)), memType == ptrType);

            for (let [ty,size,sx] of
                 [['i32','','','',],['i32','8','_u'],['i32','16','_u'],
                  ['i64','',''],['i64','8','_u'],['i64','16','_u'],['i64','32','_u']]) {
                assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory ${memType} 1 100 shared)
  (func (param $p ${ptrType}) (param $vi32 i32) (param $vi64 i64)
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.load${size}${sx} ${offs} (local.get $p))))
    (${ty}.atomic.store${size} ${offs} (local.get $p) (local.get $v${ty}))
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.rmw${size}.add${sx} ${offs} (local.get $p) (local.get $v${ty}))))
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.rmw${size}.sub${sx} ${offs} (local.get $p) (local.get $v${ty}))))
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.rmw${size}.and${sx} ${offs} (local.get $p) (local.get $v${ty}))))
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.rmw${size}.or${sx} ${offs} (local.get $p) (local.get $v${ty}))))
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.rmw${size}.xor${sx} ${offs} (local.get $p) (local.get $v${ty}))))
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.rmw${size}.xchg${sx} ${offs} (local.get $p) (local.get $v${ty}))))
    (drop (${ty}.add (${ty}.const 1) (${ty}.atomic.rmw${size}.cmpxchg${sx} ${offs} (local.get $p) (local.get $v${ty}) (${ty}.const 37))))
))`)), memType == ptrType);
            }

        }
    }
}

// Cursorily check that invalid offsets are rejected.  One day this test will
// start to fail for 64-bit memories, after we lift the implementation limit.
// When that happens, rewrite this test and expand validOffsets for i64 above.

assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory i32 1)
  (func (param $p i32)
    (drop (i32.load offset=0x100000000 (local.get $p)))))`)), false);

assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory i64 1)
  (func (param $p i64)
    (drop (i32.load offset=0x100000000 (local.get $p)))))`)), false);
