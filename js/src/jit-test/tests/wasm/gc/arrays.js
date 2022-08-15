// |jit-test| skip-if: !wasmGcEnabled()

// Test array instructions on different valtypes

const GENERAL_TESTS = [
  [
    'i32',
    0,
    0xce,
  ], [
    'i64',
    0n,
    0xabcdefn,
  ], [
    'f32',
    0,
    13.5,
  ], [
    'f64',
    0,
    13.5,
  ], [
    'externref',
    null,
    'hello',
  ],
];

for (let [valtype, def, nondef] of GENERAL_TESTS) {
  let {create, createDefault, get, set, len} = wasmEvalText(`(module
    (type $a (array (mut ${valtype})))

    (; new T[0] = { 1... }  ;)
    (func (export "create") (param i32 ${valtype}) (result eqref)
      local.get 1
      local.get 0
      array.new $a
    )

    (; new T[0] ;)
    (func (export "createDefault") (param i32) (result eqref)
      local.get 0
      array.new_default $a
    )

    (; 0[1] ;)
    (func (export "get") (param eqref i32) (result ${valtype})
      local.get 0
      ref.cast $a
      local.get 1
      array.get $a
    )

    (; 0[1] = 2 ;)
    (func (export "set") (param eqref i32 ${valtype})
      local.get 0
      ref.cast $a
      local.get 1
      local.get 2
      array.set $a
    )

    (; len(a) ;)
    (func (export "len") (param eqref) (result i32)
      local.get 0
      ref.cast $a
      array.len $a
    )
  )`).exports;

  function checkArray(array, length, init, setval) {
    // Check length
    assertEq(len(array), length);
    assertEq(array.length, length);

    // Check init value
    for (let i = 0; i < length; i++) {
      assertEq(array[i], init);
      assertEq(get(array, i), init);
    }

    // Set every element to setval
    for (let i = 0; i < length; i++) {
      set(array, i, setval);

      // Check there is no overwrite
      for (let j = i + 1; j < length; j++) {
        assertEq(array[j], init);
        assertEq(get(array, j), init);
      }
    }

    // Check out of bounds conditions
    for (let loc of [-1, length, length + 1]) {
      assertErrorMessage(() => {
        get(array, loc);
      }, WebAssembly.RuntimeError, /out of bounds/);
      assertErrorMessage(() => {
        set(array, loc, setval);
      }, WebAssembly.RuntimeError, /out of bounds/);
    }
  }

  for (let arrayLength = 0; arrayLength < 5; arrayLength++) {
    checkArray(createDefault(arrayLength), arrayLength, def, nondef);
    checkArray(create(arrayLength, nondef), arrayLength, nondef, def);
  }
}

// Check packed array reads and writes

for (let [fieldtype, max] of [
  [
    'i8',
    0xff,
  ], [
    'i16',
    0xffff,
  ]
]) {
  let {create, getS, getU, set} = wasmEvalText(`(module
    (type $a (array (mut ${fieldtype})))

    (; new T[0] = { 1... }  ;)
    (func (export "create") (param i32 i32) (result eqref)
      local.get 1
      local.get 0
      array.new $a
    )

    (; 0[1] ;)
    (func (export "getS") (param eqref i32) (result i32)
      local.get 0
      ref.cast $a
      local.get 1
      array.get_s $a
    )

    (; 0[1] ;)
    (func (export "getU") (param eqref i32) (result i32)
      local.get 0
      ref.cast $a
      local.get 1
      array.get_u $a
    )

    (; 0[1] = 2 ;)
    (func (export "set") (param eqref i32 i32)
      local.get 0
      ref.cast $a
      local.get 1
      local.get 2
      array.set $a
    )
  )`).exports;

  // Check zero and sign extension
  let a = create(1, 0);
  set(a, 0, max);
  assertEq(getS(a, 0), -1);
  assertEq(getU(a, 0), max);

  // JS-API defaults to sign extension
  assertEq(a[0], getS(a, 0));

  // Check array.new truncates init value
  assertEq(create(1, max + 1)[0], 0);

  // Check array.set truncates
  let b = create(1, 0);
  set(b, 0, max + 1);
  assertEq(getU(b, 0), 0);

  // Check no overwrite on set
  let c = create(2, 0);
  set(c, 0, max << 4);
  assertEq(getU(c, 0), (max << 4) & max);
  assertEq(getU(c, 1), 0);
}

// Check arrays must be mutable to mutate

assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i32))
    (func
      (array.set $a
        (array.new $a
          i32.const 0xff
          i32.const 10)
        i32.const 0
        i32.const 0
      )
    )
)
`), WebAssembly.CompileError, /array is not mutable/);

// Check operations trap on null

assertErrorMessage(() => {
  wasmEvalText(`(module
    (type $a (array (mut i32)))
    (func
      ref.null $a
      i32.const 0
      array.get $a
      drop
    )
    (start 0)
  )`);
}, WebAssembly.RuntimeError, /null/);

assertErrorMessage(() => {
  wasmEvalText(`(module
    (type $a (array (mut i32)))
    (func
      ref.null $a
      i32.const 0
      i32.const 0
      array.set $a
    )
    (start 0)
  )`);
}, WebAssembly.RuntimeError, /null/);

assertErrorMessage(() => {
  wasmEvalText(`(module
    (type $a (array (mut i32)))
    (func
      ref.null $a
      array.len $a
      drop
    )
    (start 0)
  )`);
}, WebAssembly.RuntimeError, /null/);

// Check an extension postfix is present iff the element is packed

for ([fieldtype, packed] of [
  ['i8', true],
  ['i16', true],
  ['i32', false],
  ['i64', false],
  ['f32', false],
  ['f64', false],
]) {
  let extensionModule = `(module
      (type $a (array ${fieldtype}))
      (func
        ref.null $a
        i32.const 0
        array.get_s $a
        drop
      )
      (func
        ref.null $a
        i32.const 0
        array.get_u $a
        drop
      )
    )`;
  let noExtensionModule = `(module
      (type $a (array ${fieldtype}))
      (func
        ref.null $a
        i32.const 0
        array.get $a
        drop
      )
    )`;

  if (packed) {
    wasmValidateText(extensionModule);
    wasmFailValidateText(noExtensionModule, /must specify signedness/);
  } else {
    wasmFailValidateText(extensionModule, /must not specify signedness/);
    wasmValidateText(noExtensionModule);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// array.new_fixed
/*
  validation:
    array-type imm-operand needs to be "in range"
    array-type imm-operand must refer to an array type
    operands (on stack) must all match ("be compatible with") the array elem
      type
    number of operands (on stack) must not be less than the num-of-elems
      imm-operand
    zero elements doesn't fail compilation
    reftypes elements doesn't fail compilation
    trying to create a 1-billion-element array fails gracefully
  run:
    resulting 4-element array is as expected
    resulting zero-element array is as expected
    resulting 30-element array is as expected
*/

// validation: array-type imm-operand needs to be "in range"
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i8))
    (func (result eqref)
      i32.const 66
      i32.const 77
      array.new_fixed 2 2  ;; type index 2 is the first invalid one
    )
)
`), WebAssembly.CompileError, /type index out of range/);

// validation: array-type imm-operand must refer to an array type
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (func (param f64) (result f64)))
    (func (result eqref)
      i32.const 66
      i32.const 77
      array.new_fixed $a 2
    )
)
`), WebAssembly.CompileError, /not an array type/);

// validation: operands (on stack) must all match ("be compatible with")
//   the array elem type
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i32))
    (func (result eqref)
      f32.const 66.6
      f64.const 77.7
      array.new_fixed $a 2
    )
)
`), WebAssembly.CompileError, /expression has type f64 but expected i32/);

// validation: number of operands (on stack) must not be less than the
//   num-of-elems imm-operand
assertNoWarning(() => wasmEvalText(`(module
    (type $a (array f32))
    (func
      f64.const 66.6  ;; we won't put this in the array
      f32.const 77.7
      f32.const 88.8
      array.new_fixed $a 2  ;; use up 88.8 and 77.7 and replace with array
      drop            ;; dump the array
      f64.const 99.9
      f64.mul         ;; check the 66.6 value is still on the stack
      drop            ;; now should be empty
    )
)
`));
// (more)
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i64))
    (func (param i64) (result eqref)
       local.get 0
       array.new_fixed $a 2
    )
)
`), WebAssembly.CompileError, /popping value from empty stack/);

// validation: zero elements doesn't fail compilation
assertNoWarning(() => wasmEvalText(`(module
    (type $a (array i32))
    (func (result eqref)
       array.new_fixed $a 0
    )
)
`));

// validation: reftyped elements doesn't fail compilation
assertNoWarning(() => wasmEvalText(`(module
    (type $a (array eqref))
    (func (param eqref) (result eqref)
       local.get 0
       array.new_fixed $a 1
    )
)
`));

// validation: trying to create a 1-billion-element array fails gracefully
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array f32))
    (func (export "newFixed") (result eqref)
            f32.const 1337.0
            f32.const 4771.0
            array.new_fixed $a 1000000000
    )
)
`), WebAssembly.CompileError, /popping value from empty stack/);

// run: resulting 4-element array is as expected
{
    let { newFixed } = wasmEvalText(`(module
        (type $a (array i8))
        (func (export "newFixed") (result eqref)
                (; the spec seems ambiguous about the operand ordering here ;)
                i32.const 66
                i32.const 77
                i32.const 88
                i32.const 99
                array.new_fixed $a 4
        )
        )`).exports;
    let a = newFixed();
    assertEq(a.length, 4);
    assertEq(a[0], 99);
    assertEq(a[1], 88);
    assertEq(a[2], 77);
    assertEq(a[3], 66);
}

// run: resulting zero-element array is as expected
{
    let { newFixed } = wasmEvalText(`(module
        (type $a (array i16))
        (func (export "newFixed") (result eqref)
                array.new_fixed $a 0
        )
        )`).exports;
    let a = newFixed();
    assertEq(a.length, 0);
}

// run: resulting 30-element array is as expected
{
    let { newFixed } = wasmEvalText(`(module
        (type $a (array i16))
        (func (export "newFixed") (result eqref)
                i32.const 1
                i32.const 2
                i32.const 3
                i32.const 4
                i32.const 5
                i32.const 6
                i32.const 7
                i32.const 8
                i32.const 9
                i32.const 10
                i32.const 11
                i32.const 12
                i32.const 13
                i32.const 14
                i32.const 15
                i32.const 16
                i32.const 17
                i32.const 18
                i32.const 19
                i32.const 20
                i32.const 21
                i32.const 22
                i32.const 23
                i32.const 24
                i32.const 25
                i32.const 26
                i32.const 27
                i32.const 28
                i32.const 29
                i32.const 30
                array.new_fixed $a 30
        )
        )`).exports;
    let a = newFixed();
    assertEq(a.length, 30);
    for (i = 0; i < 30; i++) {
        assertEq(a[i], 30 - i);
    }
}
