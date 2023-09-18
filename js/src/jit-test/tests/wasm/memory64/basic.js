// |jit-test| allow-oom

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
  new WebAssembly.Memory(memoryTypeDescriptor(shared, initial, max));
}
function invalidMemoryType(shared, initial, max, compileMessage, jsMessage) {
  wasmFailValidateText(memoryTypeModuleText(shared, initial, max), compileMessage);
  assertErrorMessage(() => wasmEvalText(memoryTypeModuleText(shared, initial, max)), WebAssembly.CompileError, compileMessage);
  // TODO: JS-API cannot specify pages above UINT32_MAX
  // https://github.com/WebAssembly/memory64/issues/24
  assertErrorMessage(() => new WebAssembly.Memory(memoryTypeDescriptor(shared, initial, max)), Error, jsMessage);
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

var validOffsets = {i32: ['', 'offset=0x10000000'],
                    i64: ['', 'offset=0x10000000', 'offset=0x200000000']}

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
  (data $seg "0123456789abcdef")
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

// Cursorily check that invalid offsets are rejected.

assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory i32 1)
  (func (param $p i32)
    (drop (i32.load offset=0x100000000 (local.get $p)))))`)), false);


// For Memory64, any valid wat-syntaxed offset is valid.

assertEq(WebAssembly.validate(wasmTextToBinary(`
(module
  (memory i64 1)
  (func (param $p i64)
    (drop (i32.load offset=0x1000000000000 (local.get $p)))))`)), true);


///////////////////////////////////////////////////////////////////////////////
//
// EXECUTION

// Smoketest: Can we actually allocate a memory larger than 4GB?

if (getBuildConfiguration("pointer-byte-size") == 8) {
    // TODO: "index" is not yet part of the spec
    // https://github.com/WebAssembly/memory64/issues/24

    try {
        new WebAssembly.Memory({index:"i64", initial:65536 * 1.5, maximum:65536 * 2});
    } catch (e) {
        // OOM is OK.
        if (!(e instanceof WebAssembly.RuntimeError) || !String(e).match(/too many memory pages/)) {
            throw e;
        }
    }
}

// JS-API

if (WebAssembly.Function) {
    // TODO: "index" is not yet part of the spec
    // TODO: values outside the u32 range are not yet part of the spec
    // https://github.com/WebAssembly/memory64/issues/24

    let m64 = new WebAssembly.Memory({index:"i64", initial:1});
    assertEq(m64.type().index, "i64");

    let m32 = new WebAssembly.Memory({initial:1});
    assertEq(m32.type().index, "i32");

    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (memory (export "mem") i64 1 0x100000000))`)));
    assertEq(ins.exports.mem.type().minimum, 1);
    assertEq(ins.exports.mem.type().maximum, 0x100000000);
}

// Instructions

const SMALL = 64;  // < offsetguard everywhere
const BIG = 131072;  // > offsetguard on 32-bit
const HUGE = 2147483656; // > offsetguard on 64-bit
const VAST = 0x112001300;   // > 4GB

function makeTest(LOC, INITIAL, MAXIMUM, SHARED) {
    const v128Prefix =
` (func $stash (param v128)
    (v128.store (i64.const 0) (local.get 0)))

  (func $unstash (result v128)
    (v128.load (i64.const 0)))
`;

    const readV128Code =
` (func (export "readv128@0") (param $p i64)
    (call $stash (v128.load (local.get $p))))

  (func (export "readv128@small") (param $p i64)
    (call $stash (v128.load offset=${SMALL} (local.get $p))))

  (func (export "readv128@big") (param $p i64)
    (call $stash (v128.load offset=${BIG} (local.get $p))))

  (func (export "readv128@huge") (param $p i64)
    (call $stash (v128.load offset=${HUGE} (local.get $p))))

  (func (export "readv128/const@0")
    (call $stash (v128.load (i64.const ${LOC}))))

  (func (export "readv128/const@small")
    (call $stash (v128.load offset=${SMALL} (i64.const ${LOC}))))

  (func (export "readv128/const@big")
    (call $stash (v128.load offset=${BIG} (i64.const ${LOC}))))

  (func (export "v128.load_splat@small") (param $p i64)
    (call $stash (v128.load32_splat offset=${SMALL} (local.get $p))))

  (func (export "v128.load_zero@small") (param $p i64)
    (call $stash (v128.load32_zero offset=${SMALL} (local.get $p))))

  (func (export "v128.load_extend@small") (param $p i64)
    (call $stash (v128.load32x2_u offset=${SMALL} (local.get $p))))

  (func (export "v128.load_lane@small") (param $p i64)
    (call $stash (v128.load32_lane offset=${SMALL} 2 (local.get $p) (call $unstash))))
`;

    const writeV128Code =
` (func (export "writev128@0") (param $p i64)
    (v128.store (local.get $p) (call $unstash)))

  (func (export "writev128@small") (param $p i64)
    (v128.store offset=${SMALL} (local.get $p) (call $unstash)))

  (func (export "writev128@big") (param $p i64)
    (v128.store offset=${BIG} (local.get $p) (call $unstash)))

  (func (export "writev128@huge") (param $p i64)
    (v128.store offset=${HUGE} (local.get $p) (call $unstash)))

  (func (export "writev128/const@0")
    (v128.store (i64.const ${LOC}) (call $unstash)))

  (func (export "writev128/const@small")
    (v128.store offset=${SMALL} (i64.const ${LOC}) (call $unstash)))

  (func (export "writev128/const@big")
    (v128.store offset=${BIG} (i64.const ${LOC}) (call $unstash)))

  (func (export "v128.store_lane@small") (param $p i64)
    (v128.store32_lane offset=${SMALL} 2 (local.get $p) (call $unstash)))
`;

    const ins = wasmEvalText(`
(module
  (memory (export "mem") i64 ${INITIAL} ${MAXIMUM} ${SHARED})

  ;; About the test cases: there are various optimizations in the engine
  ;; for different shapes of a pointer+offset.  Constant pointers are
  ;; resolved early; large offsets are folded early using explicit code
  ;; with an overflow check (but "large" depends on 32-bit vs 64-bit);
  ;; wait/notify fold offsets early regardless; zero offsets lead to
  ;; tighter code with variable pointers; and don't get me started on
  ;; alignment checks.  These test cases are not exhaustive but aim
  ;; to test at least some things.

  ;; TODO: more sizes for all operations, though this is not critical
  ;; TODO: sign extending loads, again not critical

  ${wasmSimdEnabled() ? v128Prefix : ""}

  ;; Read i32
  (func (export "readi32@0") (param $p i64) (result i32)
    (i32.load (local.get $p)))

  (func (export "readi32@small") (param $p i64) (result i32)
    (i32.load offset=${SMALL} (local.get $p)))

  (func (export "readi32@big") (param $p i64) (result i32)
    (i32.load offset=${BIG} (local.get $p)))

  (func (export "readi32@huge") (param $p i64) (result i32)
    (i32.load offset=${HUGE} (local.get $p)))

  (func (export "readi32@vast") (param $p i64) (result i32)
    (i32.load offset=${VAST} (local.get $p)))

  (func (export "readi32/const@0") (result i32)
    (i32.load (i64.const ${LOC})))

  (func (export "readi32/const@small") (result i32)
    (i32.load offset=${SMALL} (i64.const ${LOC})))

  (func (export "readi32/const@big") (result i32)
    (i32.load offset=${BIG} (i64.const ${LOC})))

  (func (export "readi32/const@vast") (result i32)
    (i32.load offset=${VAST} (i64.const ${LOC})))

  ;; Read i64
  (func (export "readi64@0") (param $p i64) (result i64)
    (i64.load (local.get $p)))

  (func (export "readi64@small") (param $p i64) (result i64)
    (i64.load offset=${SMALL} (local.get $p)))

  (func (export "readi64@big") (param $p i64) (result i64)
    (i64.load offset=${BIG} (local.get $p)))

  (func (export "readi64@huge") (param $p i64) (result i64)
    (i64.load offset=${HUGE} (local.get $p)))

  (func (export "readi64@vast") (param $p i64) (result i64)
    (i64.load offset=${VAST} (local.get $p)))

  (func (export "readi64/const@0") (result i64)
    (i64.load (i64.const ${LOC})))

  (func (export "readi64/const@small") (result i64)
    (i64.load offset=${SMALL} (i64.const ${LOC})))

  (func (export "readi64/const@big") (result i64)
    (i64.load offset=${BIG} (i64.const ${LOC})))

  (func (export "readi64/const@vast") (result i64)
    (i64.load offset=${VAST} (i64.const ${LOC})))

  ;; Read v128
  ${wasmSimdEnabled() ? readV128Code : ""}

  ;; write i32
  (func (export "writei32@0") (param $p i64) (param $v i32)
    (i32.store (local.get $p) (local.get $v)))

  (func (export "writei32@small") (param $p i64) (param $v i32)
    (i32.store offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "writei32@big") (param $p i64) (param $v i32)
    (i32.store offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "writei32@huge") (param $p i64) (param $v i32)
    (i32.store offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "writei32@vast") (param $p i64) (param $v i32)
    (i32.store offset=${VAST} (local.get $p) (local.get $v)))

  (func (export "writei32/const@0") (param $v i32)
    (i32.store (i64.const ${LOC}) (local.get $v)))

  (func (export "writei32/const@small") (param $v i32)
    (i32.store offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "writei32/const@big") (param $v i32)
    (i32.store offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  (func (export "writei32/const@vast") (param $v i32)
    (i32.store offset=${VAST} (i64.const ${LOC}) (local.get $v)))

  ;; write i64
  (func (export "writei64@0") (param $p i64) (param $v i64)
    (i64.store (local.get $p) (local.get $v)))

  (func (export "writei64@small") (param $p i64) (param $v i64)
    (i64.store offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "writei64@big") (param $p i64) (param $v i64)
    (i64.store offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "writei64@huge") (param $p i64) (param $v i64)
    (i64.store offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "writei64@vast") (param $p i64) (param $v i64)
    (i64.store offset=${VAST} (local.get $p) (local.get $v)))

  (func (export "writei64/const@0") (param $v i64)
    (i64.store (i64.const ${LOC}) (local.get $v)))

  (func (export "writei64/const@small") (param $v i64)
    (i64.store offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "writei64/const@big") (param $v i64)
    (i64.store offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  (func (export "writei64/const@vast") (param $v i64)
    (i64.store offset=${VAST} (i64.const ${LOC}) (local.get $v)))

  ;; Read v128
  ${wasmSimdEnabled() ? writeV128Code : ""}

  ;; Atomic read i32

  (func (export "areadi32@0") (param $p i64) (result i32)
    (i32.atomic.load (local.get $p)))

  (func (export "areadi32@small") (param $p i64) (result i32)
    (i32.atomic.load offset=${SMALL} (local.get $p)))

  (func (export "areadi32@big") (param $p i64) (result i32)
    (i32.atomic.load offset=${BIG} (local.get $p)))

  (func (export "areadi32@huge") (param $p i64) (result i32)
    (i32.atomic.load offset=${HUGE} (local.get $p)))

  (func (export "areadi32@vast") (param $p i64) (result i32)
    (i32.atomic.load offset=${VAST} (local.get $p)))

  (func (export "areadi32/const@0") (result i32)
    (i32.atomic.load (i64.const ${LOC})))

  (func (export "areadi32/const@small") (result i32)
    (i32.atomic.load offset=${SMALL} (i64.const ${LOC})))

  (func (export "areadi32/const@big") (result i32)
    (i32.atomic.load offset=${BIG} (i64.const ${LOC})))

  (func (export "areadi32/const@vast") (result i32)
    (i32.atomic.load offset=${VAST} (i64.const ${LOC})))

  ;; Atomic read i64

  (func (export "areadi64@0") (param $p i64) (result i64)
    (i64.atomic.load (local.get $p)))

  (func (export "areadi64@small") (param $p i64) (result i64)
    (i64.atomic.load offset=${SMALL} (local.get $p)))

  (func (export "areadi64@big") (param $p i64) (result i64)
    (i64.atomic.load offset=${BIG} (local.get $p)))

  (func (export "areadi64@huge") (param $p i64) (result i64)
    (i64.atomic.load offset=${HUGE} (local.get $p)))

  (func (export "areadi64@vast") (param $p i64) (result i64)
    (i64.atomic.load offset=${VAST} (local.get $p)))

  (func (export "areadi64/const@0") (result i64)
    (i64.atomic.load (i64.const ${LOC})))

  (func (export "areadi64/const@small") (result i64)
    (i64.atomic.load offset=${SMALL} (i64.const ${LOC})))

  (func (export "areadi64/const@big") (result i64)
    (i64.atomic.load offset=${BIG} (i64.const ${LOC})))

  (func (export "areadi64/const@vast") (result i64)
    (i64.atomic.load offset=${VAST} (i64.const ${LOC})))

  ;; Atomic write i32

  (func (export "awritei32@0") (param $p i64) (param $v i32)
    (i32.atomic.store (local.get $p) (local.get $v)))

  (func (export "awritei32@small") (param $p i64) (param $v i32)
    (i32.atomic.store offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "awritei32@big") (param $p i64) (param $v i32)
    (i32.atomic.store offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "awritei32@huge") (param $p i64) (param $v i32)
    (i32.atomic.store offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "awritei32@vast") (param $p i64) (param $v i32)
    (i32.atomic.store offset=${VAST} (local.get $p) (local.get $v)))

  (func (export "awritei32/const@0") (param $v i32)
    (i32.atomic.store (i64.const ${LOC}) (local.get $v)))

  (func (export "awritei32/const@small") (param $v i32)
    (i32.atomic.store offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "awritei32/const@big") (param $v i32)
    (i32.atomic.store offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  (func (export "awritei32/const@vast") (param $v i32)
    (i32.atomic.store offset=${VAST} (i64.const ${LOC}) (local.get $v)))

  ;; Atomic write i64

  (func (export "awritei64@0") (param $p i64) (param $v i64)
    (i64.atomic.store (local.get $p) (local.get $v)))

  (func (export "awritei64@small") (param $p i64) (param $v i64)
    (i64.atomic.store offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "awritei64@big") (param $p i64) (param $v i64)
    (i64.atomic.store offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "awritei64@huge") (param $p i64) (param $v i64)
    (i64.atomic.store offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "awritei64@vast") (param $p i64) (param $v i64)
    (i64.atomic.store offset=${VAST} (local.get $p) (local.get $v)))

  (func (export "awritei64/const@0") (param $v i64)
    (i64.atomic.store (i64.const ${LOC}) (local.get $v)))

  (func (export "awritei64/const@small") (param $v i64)
    (i64.atomic.store offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "awritei64/const@big") (param $v i64)
    (i64.atomic.store offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  (func (export "awritei64/const@vast") (param $v i64)
    (i64.atomic.store offset=${VAST} (i64.const ${LOC}) (local.get $v)))

  ;; xchg i32

  (func (export "xchgi32@0") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xchg (local.get $p) (local.get $v)))

  (func (export "xchgi32@small") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xchg offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "xchgi32@big") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xchg offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "xchgi32@huge") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xchg offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "xchgi32/const@0") (param $v i32) (result i32)
    (i32.atomic.rmw.xchg (i64.const ${LOC}) (local.get $v)))

  (func (export "xchgi32/const@small") (param $v i32) (result i32)
    (i32.atomic.rmw.xchg offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "xchgi32/const@big") (param $v i32) (result i32)
    (i32.atomic.rmw.xchg offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; xchg i64

  (func (export "xchgi64@0") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xchg (local.get $p) (local.get $v)))

  (func (export "xchgi64@small") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xchg offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "xchgi64@big") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xchg offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "xchgi64@huge") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xchg offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "xchgi64/const@0") (param $v i64) (result i64)
    (i64.atomic.rmw.xchg (i64.const ${LOC}) (local.get $v)))

  (func (export "xchgi64/const@small") (param $v i64) (result i64)
    (i64.atomic.rmw.xchg offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "xchgi64/const@big") (param $v i64) (result i64)
    (i64.atomic.rmw.xchg offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; add i32

  (func (export "addi32@0") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.add (local.get $p) (local.get $v)))

  (func (export "addi32@small") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.add offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "addi32@big") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.add offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "addi32@huge") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.add offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "addi32/const@0") (param $v i32) (result i32)
    (i32.atomic.rmw.add (i64.const ${LOC}) (local.get $v)))

  (func (export "addi32/const@small") (param $v i32) (result i32)
    (i32.atomic.rmw.add offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "addi32/const@big") (param $v i32) (result i32)
    (i32.atomic.rmw.add offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; add i64

  (func (export "addi64@0") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.add (local.get $p) (local.get $v)))

  (func (export "addi64@small") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.add offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "addi64@big") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.add offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "addi64@huge") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.add offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "addi64/const@0") (param $v i64) (result i64)
    (i64.atomic.rmw.add (i64.const ${LOC}) (local.get $v)))

  (func (export "addi64/const@small") (param $v i64) (result i64)
    (i64.atomic.rmw.add offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "addi64/const@big") (param $v i64) (result i64)
    (i64.atomic.rmw.add offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; sub i32

  (func (export "subi32@0") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.sub (local.get $p) (local.get $v)))

  (func (export "subi32@small") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.sub offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "subi32@big") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.sub offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "subi32@huge") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.sub offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "subi32/const@0") (param $v i32) (result i32)
    (i32.atomic.rmw.sub (i64.const ${LOC}) (local.get $v)))

  (func (export "subi32/const@small") (param $v i32) (result i32)
    (i32.atomic.rmw.sub offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "subi32/const@big") (param $v i32) (result i32)
    (i32.atomic.rmw.sub offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; sub i64

  (func (export "subi64@0") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.sub (local.get $p) (local.get $v)))

  (func (export "subi64@small") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.sub offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "subi64@big") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.sub offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "subi64@huge") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.sub offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "subi64/const@0") (param $v i64) (result i64)
    (i64.atomic.rmw.sub (i64.const ${LOC}) (local.get $v)))

  (func (export "subi64/const@small") (param $v i64) (result i64)
    (i64.atomic.rmw.sub offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "subi64/const@big") (param $v i64) (result i64)
    (i64.atomic.rmw.sub offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; and i32

  (func (export "andi32@0") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.and (local.get $p) (local.get $v)))

  (func (export "andi32@small") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.and offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "andi32@big") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.and offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "andi32@huge") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.and offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "andi32/const@0") (param $v i32) (result i32)
    (i32.atomic.rmw.and (i64.const ${LOC}) (local.get $v)))

  (func (export "andi32/const@small") (param $v i32) (result i32)
    (i32.atomic.rmw.and offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "andi32/const@big") (param $v i32) (result i32)
    (i32.atomic.rmw.and offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; and i64

  (func (export "andi64@0") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.and (local.get $p) (local.get $v)))

  (func (export "andi64@small") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.and offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "andi64@big") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.and offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "andi64@huge") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.and offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "andi64/const@0") (param $v i64) (result i64)
    (i64.atomic.rmw.and (i64.const ${LOC}) (local.get $v)))

  (func (export "andi64/const@small") (param $v i64) (result i64)
    (i64.atomic.rmw.and offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "andi64/const@big") (param $v i64) (result i64)
    (i64.atomic.rmw.and offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; or i32

  (func (export "ori32@0") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.or (local.get $p) (local.get $v)))

  (func (export "ori32@small") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.or offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "ori32@big") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.or offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "ori32@huge") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.or offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "ori32/const@0") (param $v i32) (result i32)
    (i32.atomic.rmw.or (i64.const ${LOC}) (local.get $v)))

  (func (export "ori32/const@small") (param $v i32) (result i32)
    (i32.atomic.rmw.or offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "ori32/const@big") (param $v i32) (result i32)
    (i32.atomic.rmw.or offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; or i64

  (func (export "ori64@0") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.or (local.get $p) (local.get $v)))

  (func (export "ori64@small") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.or offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "ori64@big") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.or offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "ori64@huge") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.or offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "ori64/const@0") (param $v i64) (result i64)
    (i64.atomic.rmw.or (i64.const ${LOC}) (local.get $v)))

  (func (export "ori64/const@small") (param $v i64) (result i64)
    (i64.atomic.rmw.or offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "ori64/const@big") (param $v i64) (result i64)
    (i64.atomic.rmw.or offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; xor i32

  (func (export "xori32@0") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xor (local.get $p) (local.get $v)))

  (func (export "xori32@small") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xor offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "xori32@big") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xor offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "xori32@huge") (param $p i64) (param $v i32) (result i32)
    (i32.atomic.rmw.xor offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "xori32/const@0") (param $v i32) (result i32)
    (i32.atomic.rmw.xor (i64.const ${LOC}) (local.get $v)))

  (func (export "xori32/const@small") (param $v i32) (result i32)
    (i32.atomic.rmw.xor offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "xori32/const@big") (param $v i32) (result i32)
    (i32.atomic.rmw.xor offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; xor i64

  (func (export "xori64@0") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xor (local.get $p) (local.get $v)))

  (func (export "xori64@small") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xor offset=${SMALL} (local.get $p) (local.get $v)))

  (func (export "xori64@big") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xor offset=${BIG} (local.get $p) (local.get $v)))

  (func (export "xori64@huge") (param $p i64) (param $v i64) (result i64)
    (i64.atomic.rmw.xor offset=${HUGE} (local.get $p) (local.get $v)))

  (func (export "xori64/const@0") (param $v i64) (result i64)
    (i64.atomic.rmw.xor (i64.const ${LOC}) (local.get $v)))

  (func (export "xori64/const@small") (param $v i64) (result i64)
    (i64.atomic.rmw.xor offset=${SMALL} (i64.const ${LOC}) (local.get $v)))

  (func (export "xori64/const@big") (param $v i64) (result i64)
    (i64.atomic.rmw.xor offset=${BIG} (i64.const ${LOC}) (local.get $v)))

  ;; cmpxchg i32

  (func (export "cmpxchgi32@0") (param $p i64) (param $expect i32) (param $new i32) (result i32)
    (i32.atomic.rmw.cmpxchg (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi32@small") (param $p i64) (param $expect i32) (param $new i32) (result i32)
    (i32.atomic.rmw.cmpxchg offset=${SMALL} (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi32@big") (param $p i64) (param $expect i32) (param $new i32) (result i32)
    (i32.atomic.rmw.cmpxchg offset=${BIG} (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi32@huge") (param $p i64) (param $expect i32) (param $new i32) (result i32)
    (i32.atomic.rmw.cmpxchg offset=${HUGE} (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi32/const@0") (param $expect i32) (param $new i32) (result i32)
    (i32.atomic.rmw.cmpxchg (i64.const ${LOC}) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi32/const@small") (param $expect i32) (param $new i32) (result i32)
    (i32.atomic.rmw.cmpxchg offset=${SMALL} (i64.const ${LOC}) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi32/const@big") (param $expect i32) (param $new i32) (result i32)
    (i32.atomic.rmw.cmpxchg offset=${BIG} (i64.const ${LOC}) (local.get $expect) (local.get $new)))

  ;; cmpxchg i64

  (func (export "cmpxchgi64@0") (param $p i64) (param $expect i64) (param $new i64) (result i64)
    (i64.atomic.rmw.cmpxchg (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi64@small") (param $p i64) (param $expect i64) (param $new i64) (result i64)
    (i64.atomic.rmw.cmpxchg offset=${SMALL} (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi64@big") (param $p i64) (param $expect i64) (param $new i64) (result i64)
    (i64.atomic.rmw.cmpxchg offset=${BIG} (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi64@huge") (param $p i64) (param $expect i64) (param $new i64) (result i64)
    (i64.atomic.rmw.cmpxchg offset=${HUGE} (local.get $p) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi64/const@0") (param $expect i64) (param $new i64) (result i64)
    (i64.atomic.rmw.cmpxchg (i64.const ${LOC}) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi64/const@small") (param $expect i64) (param $new i64) (result i64)
    (i64.atomic.rmw.cmpxchg offset=${SMALL} (i64.const ${LOC}) (local.get $expect) (local.get $new)))

  (func (export "cmpxchgi64/const@big") (param $expect i64) (param $new i64) (result i64)
    (i64.atomic.rmw.cmpxchg offset=${BIG} (i64.const ${LOC}) (local.get $expect) (local.get $new)))

  ;; wait

  (func (export "waiti32@small") (param $p i64) (result i32)
    (memory.atomic.wait32 offset=${SMALL} (local.get $p) (i32.const 1) (i64.const 0)))

  (func (export "waiti32@huge") (param $p i64) (result i32)
    (memory.atomic.wait32 offset=${HUGE} (local.get $p) (i32.const 1) (i64.const 0)))

  (func (export "waiti64@small") (param $p i64) (result i32)
    (memory.atomic.wait64 offset=${SMALL} (local.get $p) (i64.const 1) (i64.const 0)))

  (func (export "waiti64@huge") (param $p i64) (result i32)
    (memory.atomic.wait64 offset=${HUGE} (local.get $p) (i64.const 1) (i64.const 0)))

  ;; wake

  (func (export "wake@0") (param $p i64) (result i32)
    (memory.atomic.notify (local.get $p) (i32.const 1)))

  (func (export "wake@small") (param $p i64) (result i32)
    (memory.atomic.notify offset=${SMALL} (local.get $p) (i32.const 1)))

  (func (export "wake@big") (param $p i64) (result i32)
    (memory.atomic.notify offset=${BIG} (local.get $p) (i32.const 1)))

  (func (export "wake@huge") (param $p i64) (result i32)
    (memory.atomic.notify offset=${HUGE} (local.get $p) (i32.const 1)))
)
`);
    return ins;
}

function i32Random() {
    // Limit this to small positive numbers to keep life simple.
    for (;;) {
        let r = (Math.random() * 0x3FFF_FFFF) | 0;
        if (r != 0)
            return r;
    }
}

function i64Random() {
    return (BigInt(i32Random()) << 32n) | BigInt(i32Random());
}

function Random(sz) {
    if (sz == 4)
        return i32Random();
    return i64Random();
}

function Random2(sz) {
    return [Random(sz), Random(sz)];
}

function Random4(sz) {
    return [Random(sz), Random(sz), Random(sz), Random(sz)];
}

function Zero(sz) {
    if (sz == 4)
        return 0;
    return 0n;
}

function testRead(ins, mem, LOC, prefix) {
    let r = 0;
    let SZ = mem.BYTES_PER_ELEMENT;
    let len = mem.length * SZ;
    let NM = prefix + "readi" + (SZ * 8);

    // Read in-bounds

    r = Random(SZ);
    mem[LOC / SZ] = r;
    assertEq(ins.exports[NM + "@0"](BigInt(LOC)), r);
    assertEq(ins.exports[NM + "/const@0"](), r);

    mem[(len / SZ) - 1] = Zero(SZ);
    assertEq(ins.exports[NM + "@0"](BigInt(len - SZ)), Zero(SZ)); // Just barely in-bounds

    r = Random(SZ);
    mem[(LOC + SMALL) / SZ] = r;
    assertEq(ins.exports[NM + "@small"](BigInt(LOC)), r);
    assertEq(ins.exports[NM + "/const@small"](), r);

    if (len >= LOC + BIG + SZ) {
        r = Random(SZ);
        mem[(LOC + BIG) / SZ] = r;
        assertEq(ins.exports[NM + "@big"](BigInt(LOC)), r);
        assertEq(ins.exports[NM + "/const@big"](), r);
    }

    if (len >= LOC + VAST + SZ) {
        r = Random(SZ);
        mem[(LOC + VAST) / SZ] = r;
        assertEq(ins.exports[NM + "@vast"](BigInt(LOC)), r);
        assertEq(ins.exports[NM + "/const@vast"](), r);
    }

    // Read out-of-bounds

    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len)),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len-(SZ-1))),
                       WebAssembly.RuntimeError,
                       prefix == "" ? /out of bounds/ : /unaligned memory access/);

    // This is OOB if we consider the whole pointer as we must, but if we
    // mistakenly only look at the low bits then it's in-bounds.
    if (len < 0x1_0000_0000) {
        assertErrorMessage(() => ins.exports[NM + "@0"](0x1_0000_0000n),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }

    if (len < HUGE) {
        assertErrorMessage(() => ins.exports[NM + "@huge"](0n),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }

    if (len < VAST) {
        assertErrorMessage(() => ins.exports[NM + "@vast"](0n),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }
}

function testReadV128(ins, mem, LOC) {
    let r = 0;
    let SZ = mem.BYTES_PER_ELEMENT;
    let len = mem.length * SZ;
    let NM = "readv128";

    assertEq(SZ, 4);

    // Read in-bounds

    r = Random4(4);
    mem.set(r, LOC / SZ);
    ins.exports[NM + "@0"](BigInt(LOC))
    assertSame(mem.slice(0, 4), r);
    ins.exports[NM + "/const@0"]();
    assertSame(mem.slice(0, 4), r);

    r = new Int32Array([0,0,0,0]);
    mem.set(r, (len / SZ) - 4);
    ins.exports[NM + "@0"](BigInt(len - (SZ * 4)));  // Just barely in-bounds
    assertSame(mem.slice(0, 4), r);

    r = Random4(SZ);
    mem.set(r, (LOC + SMALL) / SZ);
    ins.exports[NM + "@small"](BigInt(LOC))
    assertSame(mem.slice(0, 4), r);
    ins.exports[NM + "/const@small"]();
    assertSame(mem.slice(0, 4), r);

    if (len >= LOC + BIG + SZ * 4) {
        r = Random4(SZ);
        mem.set(r, (LOC + BIG) / SZ);
        ins.exports[NM + "@big"](BigInt(LOC));
        assertSame(mem.slice(0, 4), r);
        ins.exports[NM + "/const@big"]();
        assertSame(mem.slice(0, 4), r);
    }

    // Read out-of-bounds

    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len)),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len-((SZ*4)-1))),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    // This is OOB if we consider the whole pointer as we must, but if we
    // mistakenly only look at the low bits then it's in-bounds.
    if (len < 0x1_0000_0000) {
        assertErrorMessage(() => ins.exports[NM + "@0"](0x1_0000_0000n),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }

    if (len < HUGE) {
        assertErrorMessage(() => ins.exports[NM + "@huge"](0n),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }

    // Superficial testing of other load operations

    r = i32Random()
    mem[(LOC + SMALL) / SZ] = r;
    ins.exports["v128.load_splat@small"](BigInt(LOC));
    assertSame(mem.slice(0, 4), [r, r, r, r]);

    r = i32Random()
    mem[(LOC + SMALL) / SZ] = r;
    ins.exports["v128.load_zero@small"](BigInt(LOC));
    assertSame(mem.slice(0, 4), [r, 0, 0, 0]);

    r = Random2(SZ)
    mem.set(r, (LOC + SMALL) / SZ);
    ins.exports["v128.load_extend@small"](BigInt(LOC));
    assertSame(mem.slice(0, 4), [r[0], 0, r[1], 0]);

    r = Random4(SZ)
    mem.set(r, 0);
    let s = i32Random()
    mem[(LOC + SMALL) / SZ] = s;
    ins.exports["v128.load_lane@small"](BigInt(LOC));
    assertSame(mem.slice(0, 4), [r[0], r[1], s, r[3]]);
}

function testWrite(ins, mem, LOC, prefix) {
    let r = 0;
    let SZ = mem.BYTES_PER_ELEMENT;
    let len = mem.length * SZ;
    let WNM = prefix + "writei" + (SZ * 8);
    let RNM = prefix + "readi" + (SZ * 8);

    // Write in-bounds

    r = Random(SZ);
    ins.exports[WNM + "@0"](BigInt(LOC), r);
    assertEq(ins.exports[RNM + "@0"](BigInt(LOC)), r);

    r = Random(SZ);
    ins.exports[WNM + "@small"](BigInt(LOC), r);
    assertEq(ins.exports[RNM + "@small"](BigInt(LOC)), r);

    if (len >= LOC + BIG + SZ) {
        r = Random(SZ);
        ins.exports[WNM + "@big"](BigInt(LOC), r);
        assertEq(ins.exports[RNM + "@big"](BigInt(LOC)), r);
    }

    if (len >= LOC + VAST + SZ) {
        r = Random(SZ);
        ins.exports[WNM + "@vast"](BigInt(LOC), r);
        assertEq(ins.exports[RNM + "@vast"](BigInt(LOC)), r);
    }

    r = Random(SZ);
    ins.exports[WNM + "@0"](BigInt(len - SZ), r); // Just barely in-bounds
    assertEq(ins.exports[RNM + "@0"](BigInt(len - SZ)), r);

    // Write out-of-bounds

    assertErrorMessage(() => ins.exports[WNM + "@0"](BigInt(len), Random(SZ)),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    assertErrorMessage(() => ins.exports[WNM + "@0"](BigInt(len - (SZ - 1)), Random(SZ)),
                       WebAssembly.RuntimeError,
                       prefix == "" ? /out of bounds/ : /unaligned memory access/);

    if (len < 0x1_0000_0000) {
        let xs = ins.exports[RNM + "@0"](0n);
        assertErrorMessage(() => ins.exports[WNM + "@0"](0x1_0000_0000n, Random(SZ)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
        // Check for scribbling
        assertEq(ins.exports[RNM + "@0"](0n), xs);
    }

    if (len < HUGE) {
        assertErrorMessage(() => ins.exports[WNM + "@huge"](0n, Random(SZ)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }

    if (len < VAST) {
        assertErrorMessage(() => ins.exports[WNM + "@vast"](0n, Random(SZ)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }
}

function testWriteV128(ins, mem, LOC) {
    let r = 0;
    let p = 0;
    let SZ = mem.BYTES_PER_ELEMENT;
    let len = mem.length * SZ;
    let WNM = "writev128";
    let RNM = "readv128";

    assertEq(SZ, 4);

    // Write in-bounds

    r = Random4(SZ);
    mem.set(r, 0);
    p = LOC / SZ;
    ins.exports[WNM + "@0"](BigInt(LOC));
    assertSame(mem.slice(p, p + 4), r);

    r = Random4(SZ);
    mem.set(r, 0);
    p = (LOC + SMALL) / SZ;
    ins.exports[WNM + "@small"](BigInt(LOC));
    assertSame(mem.slice(p, p + 4), r);

    if (len >= LOC + BIG + SZ) {
        r = Random4(SZ);
        mem.set(r, 0);
        p = (LOC + BIG) / SZ;
        ins.exports[WNM + "@big"](BigInt(LOC));
        assertSame(mem.slice(p, p + 4), r);
    }

    r = Random4(SZ);
    mem.set(r, 0);
    p = (len - (SZ * 4)) / SZ;
    ins.exports[WNM + "@0"](BigInt(len - (SZ * 4))); // Just barely in-bounds
    assertSame(mem.slice(p, p + 4), r);

    // Write out-of-bounds

    assertErrorMessage(() => ins.exports[WNM + "@0"](BigInt(len)),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    assertErrorMessage(() => ins.exports[WNM + "@0"](BigInt(len - ((SZ * 4) - 1))),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    if (len < HUGE) {
        assertErrorMessage(() => ins.exports[WNM + "@huge"](0n),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }

    // Superficial testing of other store operations

    r = Random4(SZ);
    mem.set(r, 0);
    ins.exports["v128.store_lane@small"](BigInt(LOC));
    assertEq(mem[(LOC + SMALL) / SZ], r[2]);
}

function testAtomicRMW(ins, mem, LOC, op, fn) {
    let r = 0, s = 0;
    let SZ = mem.BYTES_PER_ELEMENT;
    let len = mem.length * SZ;
    let NM = op + "i" + (SZ * 8);

    [r,s] = Random2(SZ);
    mem[LOC / SZ] = r;
    assertEq(ins.exports[NM + "@0"](BigInt(LOC), s), r);
    assertEq(mem[LOC / SZ], fn(r, s));

    [r,s] = Random2(SZ);
    mem[(LOC + SMALL) / SZ] = r;
    assertEq(ins.exports[NM + "@small"](BigInt(LOC), s), r);
    assertEq(mem[(LOC + SMALL) / SZ], fn(r, s));

    if (len >= LOC + BIG + SZ) {
        [r,s] = Random2(SZ);
        mem[(LOC + BIG) / SZ] = r;
        assertEq(ins.exports[NM + "@big"](BigInt(LOC), s), r);
        assertEq(mem[(LOC + BIG) / SZ], fn(r, s));
    }


    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len), Zero(SZ)),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len - (SZ - 1)), Zero(SZ)),
                       WebAssembly.RuntimeError,
                       /unaligned memory access/);

    if (len < HUGE) {
        assertErrorMessage(() => ins.exports[NM + "@huge"](0n, Zero(SZ)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }
}

function testAtomicCmpxchg(ins, mem, LOC) {
    let r = 0, s = 0;
    let SZ = mem.BYTES_PER_ELEMENT;
    let len = mem.length * SZ;
    let NM = "cmpxchgi" + (SZ * 8);

    [r,s] = Random2(SZ);
    mem[LOC / SZ] = r;
    assertEq(ins.exports[NM + "@0"](BigInt(LOC), Zero(SZ), s), r);
    assertEq(ins.exports[NM + "@0"](BigInt(LOC), r, s), r);
    assertEq(mem[LOC / SZ], s);

    [r,s] = Random2(SZ);
    mem[(LOC + SMALL) / SZ] = r;
    assertEq(ins.exports[NM + "@0"](BigInt(LOC + SMALL), Zero(SZ), s), r);
    assertEq(ins.exports[NM + "@0"](BigInt(LOC + SMALL), r, s), r);
    assertEq(mem[(LOC + SMALL) / SZ], s);

    if (len >= LOC + BIG + SZ) {
        [r,s] = Random2(SZ);
        mem[(LOC + BIG) / SZ] = r;
        assertEq(ins.exports[NM + "@0"](BigInt(LOC + BIG), Zero(SZ), s), r);
        assertEq(ins.exports[NM + "@0"](BigInt(LOC + BIG), r, s), r);
        assertEq(mem[(LOC + BIG) / SZ], s);
    }

    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len), Zero(SZ), Zero(SZ)),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    assertErrorMessage(() => ins.exports[NM + "@0"](BigInt(len - (SZ - 1)), Zero(SZ), Zero(SZ)),
                       WebAssembly.RuntimeError,
                       /unaligned memory access/);

    if (len < HUGE) {
        assertErrorMessage(() => ins.exports[NM + "@huge"](0n, Zero(SZ), Zero(SZ)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }
}

function testAtomicWake(ins, mem, LOC) {
    let SZ = mem.BYTES_PER_ELEMENT;
    let len = mem.length * SZ;

    assertEq(ins.exports["wake@0"](BigInt(LOC)), 0);
    assertEq(ins.exports["wake@small"](BigInt(LOC)), 0);
    if (len >= LOC + BIG + SZ) {
        assertEq(ins.exports["wake@big"](BigInt(LOC)), 0);
    }

    assertErrorMessage(() => ins.exports["wake@0"](BigInt(len)),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    assertErrorMessage(() => ins.exports["wake@0"](BigInt(len - (SZ - 1))),
                       WebAssembly.RuntimeError,
                       /unaligned memory access/);

    if (len < HUGE) {
        assertErrorMessage(() => ins.exports["wake@huge"](BigInt(LOC)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
    }
}

// Sometimes we start memory at zero to disable certain bounds checking
// optimizations.  Other times we start memory at something beyond most of
// our references to enable those optimizations.
let configs = [[40, 0, 3], [40, 3, '']];

// On 64-bit systems, test beyond 2GB and also beyond 4GB
if (getBuildConfiguration("pointer-byte-size") == 8) {
    configs.push([Math.pow(2, 31) + 40, 32771, '']);
    configs.push([Math.pow(2, 32) + 40, 65539, '']);
    configs.push([Math.pow(2, 31) + 40, 32771, 32773]);
    configs.push([Math.pow(2, 32) + 40, 65539, 65541]);
}

for ( let shared of ['','shared'] ) {
    for (let [LOC, start, max] of configs) {
        if (shared != '' && max == '') {
            continue;
        }
        const ins = makeTest(LOC, start, max, shared);
        if (max != '') {
            // This can OOM legitimately; let it.
            let res = ins.exports.mem.grow(max - start);
            if (res == -1) {
                print("SPURIOUS OOM");
                continue;
            }
            assertEq(res, start);
        }

        const mem32 = new Int32Array(ins.exports.mem.buffer);
        const mem64 = new BigInt64Array(ins.exports.mem.buffer);

        for ( let m of [mem32, mem64] ) {
            testRead(ins, m, LOC, "");
            testWrite(ins, m, LOC, "");
            testRead(ins, m, LOC, "a");
            testWrite(ins, m, LOC, "a");
            testAtomicRMW(ins, m, LOC, "add", (r,s) => r+s);
            testAtomicRMW(ins, m, LOC, "sub", (r,s) => r-s);
            testAtomicRMW(ins, m, LOC, "and", (r,s) => r&s);
            testAtomicRMW(ins, m, LOC, "or", (r,s) => r|s);
            testAtomicRMW(ins, m, LOC, "xor", (r,s) => r^s);
            testAtomicRMW(ins, m, LOC, "xchg", (r,s) => s);
            testAtomicCmpxchg(ins, m, LOC);
            testAtomicWake(ins, m, LOC);
        }

        if (wasmSimdEnabled()) {
            testReadV128(ins, mem32, LOC);
            testWriteV128(ins, mem32, LOC);
        }
    }
}

// Bulk memory operations

function makeModule(initial, maximum, shared) {
    return `
(module
  (memory (export "mem") i64 ${initial} ${maximum} ${shared})

  (data $seg "0123456789")

  (func (export "size") (result i64)
    memory.size)

  (func (export "grow") (param $delta i64) (result i64)
    (memory.grow (local.get $delta)))

  (func (export "copy") (param $to i64) (param $from i64) (param $len i64)
    (memory.copy (local.get $to) (local.get $from) (local.get $len)))

  (func (export "fill") (param $to i64) (param $val i32) (param $len i64)
    (memory.fill (local.get $to) (local.get $val) (local.get $len)))

  (func (export "init") (param $to i64) (param $src i32) (param $count i32)
    (memory.init $seg (local.get $to) (local.get $src) (local.get $count)))
)`;
}

for ( let shared of ['','shared'] ) {
    let ins = wasmEvalText(makeModule(1, 3, shared));

    assertEq(ins.exports.size(), 1n);

    // OOM with very low probability will result in test failure
    assertEq(ins.exports.grow(2n), 1n);
    assertEq(ins.exports.size(), 3n);

    // OOM with very low probability will result in test failure
    assertEq(ins.exports.grow(1n), -1n);
    assertEq(ins.exports.size(), 3n);

    // More than max pages
    assertEq(ins.exports.grow(100000n), -1n);
    assertEq(ins.exports.size(), 3n);

    // More than 2^48 pages
    assertEq(ins.exports.grow(0x1_0000_0000_0000n), -1n);
    assertEq(ins.exports.size(), 3n);

    // More than 2^48 pages - interpreted as unsigned
    assertEq(ins.exports.grow(-1n), -1n);
    assertEq(ins.exports.size(), 3n);

    var mem = new Uint8Array(ins.exports.mem.buffer);
    var val = [1,2,3,4,5];
    mem.set(val, 20);
    ins.exports.copy(40n, 20n, 5n);
    assertSame(val, mem.slice(40, 45));

    ins.exports.fill(39n, 37, 8n);
    assertSame(iota(8).map(_ => 37), mem.slice(39, 47));

    ins.exports.init(128n, 1, 5);
    assertSame(iota(5).map(x => x+49), mem.slice(128, 133));
}

if (getBuildConfiguration("pointer-byte-size") == 8) {
    for ( let shared of ['','shared'] ) {
        let limit = wasmMaxMemoryPages('i64');
        let initial = 65537;
        let maximum = limit + 1;
        let pagesize = 65536n;

        let ins = wasmEvalText(makeModule(initial, maximum, shared));

        assertEq(ins.exports.size(), BigInt(initial));

        // This can OOM legitimately; let it.
        {
            let res = ins.exports.grow(2n);
            if (res == -1) {
                print("SPURIOUS OOM");
                continue;
            }
            assertEq(res, BigInt(initial));
        }
        assertEq(ins.exports.size(), BigInt(initial + 2));

        // This must fail
        assertEq(ins.exports.grow(BigInt(limit - (initial + 2) + 1)), -1n);
        assertEq(ins.exports.size(), BigInt(initial + 2));

        // This can OOM legitimately; let it.
        {
            let res = ins.exports.grow(BigInt(limit - (initial + 2)));
            if (res == -1) {
                print("SPURIOUS OOM");
                continue;
            }
            assertEq(res, BigInt(initial + 2));
        }
        assertEq(ins.exports.size(), BigInt(limit));

        let mem = new Uint8Array(ins.exports.mem.buffer);

        let copyval = [1,2,3,4,5];
        let source = 20n;
        let target = BigInt(initial) * pagesize + 40n;
        let oobTarget = BigInt(limit) * pagesize - 1n;

        // Copy from memory below 4GB to memory beyond 4GB
        mem.set(copyval, Number(source));
        ins.exports.copy(target, source, BigInt(copyval.length));
        assertSame(copyval, mem.slice(Number(target), Number(target) + copyval.length))

        // Try to copy out of bounds
        // src and target are both in bounds but len brings it oob
        assertErrorMessage(() => ins.exports.copy(oobTarget, source, BigInt(copyval.length)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
        assertEq(mem[Number(oobTarget-1n)], 0);
        assertErrorMessage(() => ins.exports.copy(-1n, source, BigInt(copyval.length)),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
        assertEq(mem[Number(oobTarget-1n)], 0);

        // Fill above 4GB
        ins.exports.fill(target, 37, 30n);
        assertSame(iota(30).map(_ => 37), mem.slice(Number(target), Number(target) + 30));

        // Try to fill out of bounds
        assertErrorMessage(() => ins.exports.fill(oobTarget, 37, 2n),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
        assertEq(mem[Number(oobTarget-1n)], 0);

        // Init above 4GB
        ins.exports.init(target, 1, 5);
        assertSame(iota(5).map(x => x+49), mem.slice(Number(target), Number(target)+5));

        // Try to init out of bounds
        assertErrorMessage(() => ins.exports.init(oobTarget, 1, 5),
                           WebAssembly.RuntimeError,
                           /out of bounds/);
        assertEq(mem[Number(oobTarget-1n)], 0);
    }
}
