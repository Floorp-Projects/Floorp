// |jit-test| skip-if: !wasmGcEnabled()
//
// Checks if null dereference works.

for (let [fieldType, signedness, defaultValue] of [
  ['i8', '_u', 'i32.const 42'],
  ['i8', '_s', 'i32.const -42'],
  ['i16', '_u', 'i32.const 1'],
  ['i16', '_s', 'i32.const -1'],
  ['i32', '', 'i32.const 3'],
  ['i64', '', 'i64.const -77777777777'],
  ['f32', '', 'f32.const 1.4'],
  ['f64', '', 'f64.const 3.14'],
  ['externref', '', 'ref.null extern'],
].concat(
  wasmSimdEnabled() ? [['v128', '', 'v128.const i32x4 1 2 -3 4']] : []
)) {
  // Check struct.get from null struct of a field of fieldType works
  // Check struct.set similarly
  testStructGetSet(fieldType, signedness, defaultValue, 4);
  //   - Also when the field is in the outline_ data area?
  testStructGetSet(fieldType, signedness, defaultValue, 1000);
  // Check array.get similarly
  // Check array.set from null array with element of type fieldType
  testArrayGetSet(fieldType, signedness, defaultValue, 100);
}

function testStructGetSet(fieldType, signedness, defaultValue, numFields) {
  const ins = wasmEvalText(`
(module
  (type $t (struct ${
      Array(numFields).fill("(field (mut " + fieldType + "))").join(' ')
  }))
  (global $g (mut (ref null $t)) (ref.null $t))
  (func (export "init-null")
    ref.null $t
    global.set $g
  )
  (func (export "init-non-null")
    ${ Array(numFields).fill(defaultValue).join('\n    ') }
    struct.new $t
    global.set $g
  )
  (func (export "test_get_first")
    global.get $g
    struct.get${signedness} $t 0
    drop
  )
  (func (export "test_set_first")
    global.get $g
    ${defaultValue}
    struct.set $t 0
  )
  (func (export "test_get_mid")
    global.get $g
    struct.get${signedness} $t ${numFields >> 1}
    drop
  )
  (func (export "test_set_mid")
    global.get $g
    ${defaultValue}
    struct.set $t ${numFields >> 1}
  )
  (func (export "test_get_last")
    global.get $g
    struct.get${signedness} $t ${numFields - 1}
    drop
  )
  (func (export "test_set_last")
    global.get $g
    ${defaultValue}
    struct.set $t ${numFields - 1}
  )
)`);
  ins.exports["init-non-null"]();
  ins.exports["test_get_first"]();
  ins.exports["test_get_mid"]();
  ins.exports["test_get_last"]();
  ins.exports["test_set_first"]();
  ins.exports["test_set_mid"]();
  ins.exports["test_set_last"]();

  ins.exports["init-null"]();
  assertDereferenceNull(() => ins.exports["test_get_first"]());
  assertDereferenceNull(() => ins.exports["test_get_mid"]());
  assertDereferenceNull(() => ins.exports["test_get_last"]());
  assertDereferenceNull(() => ins.exports["test_set_first"]());
  assertDereferenceNull(() => ins.exports["test_set_mid"]());
  assertDereferenceNull(() => ins.exports["test_set_last"]());

  ins.exports["init-non-null"]();
  ins.exports["test_set_last"]();
  ins.exports["test_get_first"]();
}

function testArrayGetSet(fieldType, signedness, defaultValue, numItems) {
  const ins = wasmEvalText(`
(module
  (type $t (array (mut ${fieldType})))
  (global $g (mut (ref null $t)) (ref.null $t))
  (func (export "init-null")
    ref.null $t
    global.set $g
  )
  (func (export "init-non-null")
    ${defaultValue}
    i32.const ${numItems}
    array.new $t
    global.set $g
  )
  (func (export "test_get") (param i32)
    global.get $g
    local.get 0
    array.get${signedness} $t
    drop
  )
  (func (export "test_set") (param i32)
    global.get $g
    local.get 0
    ${defaultValue}
    array.set $t
  )
)`);
  ins.exports["init-non-null"]();
  ins.exports["test_get"](0);
  ins.exports["test_get"](numItems >> 1);
  ins.exports["test_get"](numItems - 1);
  ins.exports["test_set"](0);
  ins.exports["test_set"](numItems >> 1);
  ins.exports["test_set"](numItems - 1);

  ins.exports["init-null"]();
  assertDereferenceNull(() => ins.exports["test_get"](0));
  assertDereferenceNull(() => ins.exports["test_get"](numItems >> 1));
  assertDereferenceNull(() => ins.exports["test_get"](numItems - 1));
  assertDereferenceNull(() => ins.exports["test_set"](0));
  assertDereferenceNull(() => ins.exports["test_set"](numItems >> 1));
  assertDereferenceNull(() => ins.exports["test_set"](numItems - 1));

  ins.exports["init-non-null"]();
  ins.exports["test_set"](3);
  ins.exports["test_get"](0);
}


function assertDereferenceNull(fun) {
  assertErrorMessage(fun, WebAssembly.RuntimeError, /dereferencing null pointer/);
}

// Linear memory loads/stores from small constant addresses also require
// trapsites, it seems.  So check that the following is compilable -- in
// particular, that it doesn't produce any TrapSite placement validation errors.

const ins = wasmEvalText(`
  (module
    (memory 1)
    (func (result i32) (i32.load8_s (i32.const 17)))
    (func (result i32) (i32.load8_u (i32.const 17)))
    (func (result i32) (i32.load16_s (i32.const 17)))
    (func (result i32) (i32.load16_u (i32.const 17)))

    (func (result i64) (i64.load8_s (i32.const 17)))
    (func (result i64) (i64.load8_u (i32.const 17)))
    (func (result i64) (i64.load16_s (i32.const 17)))
    (func (result i64) (i64.load16_u (i32.const 17)))

    (func (result i64) (i64.load32_s (i32.const 17)))
    (func (result i64) (i64.load32_u (i32.const 17)))

    (func (param i32) (i32.store8  (i32.const 17) (local.get 0)))
    (func (param i32) (i32.store16 (i32.const 17) (local.get 0)))

    (func (param i64) (i64.store8  (i32.const 17) (local.get 0)))
    (func (param i64) (i64.store16 (i32.const 17) (local.get 0)))
    (func (param i64) (i64.store32 (i32.const 17) (local.get 0)))

    (func (export "leet") (result i32) (i32.const 1337))
  )`);

assertEq(ins.exports["leet"](), 1337);
