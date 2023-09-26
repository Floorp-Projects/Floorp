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
      ref.cast (ref null $a)
      local.get 1
      array.get $a
    )

    (; 0[1] = 2 ;)
    (func (export "set") (param eqref i32 ${valtype})
      local.get 0
      ref.cast (ref null $a)
      local.get 1
      local.get 2
      array.set $a
    )

    (; len(a) ;)
    (func (export "len") (param eqref) (result i32)
      local.get 0
      ref.cast (ref null $a)
      array.len
    )
  )`).exports;

  function checkArray(array, length, init, setval) {
    // Check length
    assertEq(len(array), length);
    assertEq(wasmGcArrayLength(array), length);

    // Check init value
    for (let i = 0; i < length; i++) {
      assertEq(wasmGcReadField(array, i), init);
      assertEq(get(array, i), init);
    }

    // Set every element to setval
    for (let i = 0; i < length; i++) {
      set(array, i, setval);

      // Check there is no overwrite
      for (let j = i + 1; j < length; j++) {
        assertEq(wasmGcReadField(array, j), init);
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
      ref.cast (ref null $a)
      local.get 1
      array.get_s $a
    )

    (; 0[1] ;)
    (func (export "getU") (param eqref i32) (result i32)
      local.get 0
      ref.cast (ref null $a)
      local.get 1
      array.get_u $a
    )

    (; 0[1] = 2 ;)
    (func (export "set") (param eqref i32 i32)
      local.get 0
      ref.cast (ref null $a)
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
  assertEq(wasmGcReadField(a, 0), getS(a, 0));

  // Check array.new truncates init value
  assertEq(wasmGcReadField(create(1, max + 1), 0), 0);

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
      array.len
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

// run: resulting 4-element array is as expected
{
    let { newFixed } = wasmEvalText(`(module
        (type $a (array i8))
        (func (export "newFixed") (result eqref)
                i32.const 66
                i32.const 77
                i32.const 88
                i32.const 99
                array.new_fixed $a 4
        )
        )`).exports;
    let a = newFixed();
    assertEq(wasmGcArrayLength(a), 4);
    assertEq(wasmGcReadField(a, 0), 66);
    assertEq(wasmGcReadField(a, 1), 77);
    assertEq(wasmGcReadField(a, 2), 88);
    assertEq(wasmGcReadField(a, 3), 99);
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
    assertEq(wasmGcArrayLength(a), 0);
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
    assertEq(wasmGcArrayLength(a), 30);
    for (i = 0; i < 30; i++) {
        assertEq(wasmGcReadField(a, i), i + 1);
    }
}

// run: resulting 10_000-element array is as expected
{
    let initializers = '';
    for (let i = 0; i < 10_000; i++) {
      initializers += `i32.const ${i + 1}\n`;
    }
    let { newFixed } = wasmEvalText(`(module
        (type $a (array i16))
        (func (export "newFixed") (result eqref)
                ${initializers}
                array.new_fixed $a 10000
        )
        )`).exports;
    let a = newFixed();
    assertEq(wasmGcArrayLength(a), 10000);
    for (i = 0; i < 10000; i++) {
        assertEq(wasmGcReadField(a, i), i + 1);
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// array.new_data
/*
  validation:
    array-type imm-operand needs to be "in range"
    array-type imm-operand must refer to an array type
    array-type imm-operand must refer to an array of numeric elements
    segment index must be "in range"
  run:
    if segment is "already used" (active, or passive that has subsequently
      been dropped), then only a zero length array can be created
    range to copy would require OOB read on data segment
    resulting array is as expected
*/

// validation: array-type imm-operand needs to be "in range"
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (array i8))
        (data $d "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 0
                (; size=4 into data ;) i32.const 4
                array.new_data 2 $d
        )
)`), WebAssembly.CompileError, /type index out of range/);

// validation: array-type imm-operand must refer to an array type
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (func (param f32) (result f32)))
        (data $d "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 0
                (; size=4 into data ;) i32.const 4
                array.new_data $a $d
        )
)`), WebAssembly.CompileError, /not an array type/);

// validation: array-type imm-operand must refer to an array of numeric elements
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (array eqref))
        (data $d "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 0
                (; size=4 into data ;) i32.const 4
                array.new_data $a $d
        )
)`), WebAssembly.CompileError,
                   /element type must be i8\/i16\/i32\/i64\/f32\/f64\/v128/);

// validation: segment index must be "in range"
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (array i8))
        (data $d "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 0
                (; size=4 into data ;) i32.const 4
                array.new_data $a 1  ;; 1 is the lowest invalid dseg index
        )
)`), WebAssembly.CompileError, /segment index is out of range/);

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #1
{
    let { newData } = wasmEvalText(`(module
        (memory 1)
        (type $a (array i8))
        (data $d (offset (i32.const 0)) "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 0
                (; size=4 into data ;) i32.const 4
                array.new_data $a $d
        )
        )`).exports;
    assertErrorMessage(() => {
        newData();
    },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #2
{
    let { newData } = wasmEvalText(`(module
        (memory 1)
        (type $a (array i8))
        (data $d (offset (i32.const 0)) "1337")
        (func (export "newData") (result eqref)
                (; offset=4 into data ;) i32.const 4
                (; size=0 into data ;) i32.const 0
                array.new_data $a $d
        )
        )`).exports;
    assertErrorMessage(() => {
        newData();
    },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #3
{
    let { newData } = wasmEvalText(`(module
        (memory 1)
        (type $a (array i8))
        (data $d (offset (i32.const 0)) "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 0
                (; size=0 into data ;) i32.const 0
                array.new_data $a $d
        )
        )`).exports;
    let arr = newData();
    assertEq(wasmGcArrayLength(arr), 0);
}

// run: range to copy would require OOB read on data segment
{
    let { newData } = wasmEvalText(`(module
        (type $a (array i8))
        (data $d "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 1
                (; size=4 into data ;) i32.const 4
                array.new_data $a $d
        )
        )`).exports;
    assertErrorMessage(() => {
        newData();
    },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: resulting array is as expected
{
    let { newData } = wasmEvalText(`(module
        (type $a (array i8))
        (data $other "\\\\9")
        (data $d "1337")
        (func (export "newData") (result eqref)
                (; offset=0 into data ;) i32.const 0
                (; size=4 into data ;) i32.const 4
                array.new_data $a $d
        )
        )`).exports;
    let arr = newData();
    assertEq(wasmGcArrayLength(arr), 4);
    assertEq(wasmGcReadField(arr, 0), 48+1);
    assertEq(wasmGcReadField(arr, 1), 48+3);
    assertEq(wasmGcReadField(arr, 2), 48+3);
    assertEq(wasmGcReadField(arr, 3), 48+7);
}

//////////////////////////////////////////////////////////////////////////////
//
// array.new_elem
/*
  validation:
    array-type imm-operand needs to be "in range"
    array-type imm-operand must refer to an array type
    array-type imm-operand must refer to an array of ref typed elements
    destination elem type must be a supertype of src elem type
    segment index must be "in range"
  run:
    if segment is "already used" (active, or passive that has subsequently
      been dropped), then only a zero length array can be created
    range to copy would require OOB read on elem segment
    resulting array is as expected
*/

// validation: array-type imm-operand needs to be "in range"
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (array funcref))
        (elem $e func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=4 into elem ;) i32.const 4
                array.new_elem 3 $e
        )
)`), WebAssembly.CompileError, /type index out of range/);

// validation: array-type imm-operand must refer to an array type
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (func (param i64) (result f64)))
        (elem $e func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=4 into elem ;) i32.const 4
                array.new_elem $a $e
        )
)`), WebAssembly.CompileError, /not an array type/);

// validation: array-type imm-operand must refer to an array of ref typed
//   elements
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (array f32))
        (elem $e func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=4 into elem ;) i32.const 4
                array.new_elem $a $e
        )
)`), WebAssembly.CompileError, /element type is not a reftype/);

// validation: destination elem type must be a supertype of src elem type
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (array eqref))
        (elem $e func $f1)
        (func $f1 (export "f1"))
        ;; The implied copy here is from elem-seg-of-funcrefs to
        ;; array-of-eqrefs, which must fail, because funcref isn't
        ;; a subtype of eqref.
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=0 into elem ;) i32.const 0
                array.new_elem $a $e
        )
)`), WebAssembly.CompileError, /incompatible element types/);

// validation: segment index must be "in range"
assertErrorMessage(() => wasmEvalText(`(module
        (type $a (array funcref))
        (elem $e func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=4 into elem ;) i32.const 4
                array.new_elem $a 1  ;; 1 is the lowest invalid eseg index
        )
)`), WebAssembly.CompileError, /segment index is out of range/);

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #1
{
    let { newElem } = wasmEvalText(`(module
        (table 4 funcref)
        (type $a (array funcref))
        (elem $e (offset (i32.const 0)) func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=4 into elem ;) i32.const 4
                array.new_elem $a $e
        )
        )`).exports;
    assertErrorMessage(() => {
        newElem();
    }, WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #2
{
    let { newElem } = wasmEvalText(`(module
        (table 4 funcref)
        (type $a (array funcref))
        (elem $e (offset (i32.const 0)) func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=4 into elem ;) i32.const 4
                (; size=0 into elem ;) i32.const 0
                array.new_elem $a $e
        )
        )`).exports;
    assertErrorMessage(() => {
        newElem();
    }, WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #3
{
    let { newElem } = wasmEvalText(`(module
        (table 4 funcref)
        (type $a (array funcref))
        (elem $e (offset (i32.const 0)) func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=0 into elem ;) i32.const 0
                array.new_elem $a $e
        )
        )`).exports;
    let arr = newElem();
    assertEq(wasmGcArrayLength(arr), 0);
}

// run: range to copy would require OOB read on elem segment
{
    let { newElem } = wasmEvalText(`(module
        (type $a (array funcref))
        (elem $e func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 1
                (; size=4 into elem ;) i32.const 4
                array.new_elem $a $e
        )
        )`).exports;
    assertErrorMessage(() => {
        newElem();
    },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: resulting array is as expected
{
    let { newElem, f1, f2, f3, f4 } = wasmEvalText(`(module
        (type $a (array funcref))
        (elem $e func $f1 $f2 $f3 $f4)
        (func $f1 (export "f1"))
        (func $f2 (export "f2"))
        (func $f3 (export "f3"))
        (func $f4 (export "f4"))
        (func (export "newElem") (result eqref)
                (; offset=0 into elem ;) i32.const 0
                (; size=4 into elem ;) i32.const 4
                array.new_elem $a $e
        )
        )`).exports;
    let arr = newElem();
    assertEq(wasmGcArrayLength(arr), 4);
    assertEq(wasmGcReadField(arr, 0), f1);
    assertEq(wasmGcReadField(arr, 1), f2);
    assertEq(wasmGcReadField(arr, 2), f3);
    assertEq(wasmGcReadField(arr, 3), f4);
}

//////////////////////////////////////////////////////////////////////////////
//
// array.init_data
/*
  validation:
    array-type imm-operand needs to be "in range"
    array-type imm-operand must refer to an array type
    array-type imm-operand must refer to an array of numeric elements
    array-type imm-operand must be mutable
    segment index must be "in range"
  run:
    array must not be null
    if segment is "already used" (active, or passive that has subsequently
      been dropped), then only a zero length array can be created
    range to copy would require OOB read on data segment
    range to copy would require OOB write on array
    resulting array is as expected
*/

// validation: array-type imm-operand needs to be "in range"
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array i8))
  (data $d "1337")
  (func (export "initData")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into data ;)  i32.const 0
    (; size=4 elements ;)     i32.const 4
    array.init_data 2 $d
  )
)`), WebAssembly.CompileError, /type index out of range/);

// validation: array-type imm-operand must refer to an array type
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (func (param f32) (result f32)))
  (data $d "1337")
  (func (export "initData")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into data ;)  i32.const 0
    (; size=4 elements ;)     i32.const 4
    array.init_data $a $d
  )
)`), WebAssembly.CompileError, /not an array type/);

// validation: array-type imm-operand must refer to an array of numeric elements
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array eqref))
  (data $d "1337")
  (func (export "initData")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into data ;)  i32.const 0
    (; size=4 elements ;)     i32.const 4
    array.init_data $a $d
  )
)`), WebAssembly.CompileError, /element type must be i8\/i16\/i32\/i64\/f32\/f64\/v128/);

// validation: array-type imm-operand must be mutable
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array i8))
  (data $d "1337")
  (func (export "initData")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into data ;)  i32.const 0
    (; size=4 elements ;)     i32.const 4
    array.init_data $a $d
  )
)`), WebAssembly.CompileError,
             /destination array is not mutable/);

// validation: segment index must be "in range"
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array (mut i8)))
  (data $d "1337")
  (func (export "initData")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into data ;)  i32.const 0
    (; size=4 elements ;)     i32.const 4
    array.init_data $a 1  ;; 1 is the lowest invalid dseg index
  )
  (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
)`), WebAssembly.CompileError, /segment index is out of range/);

// run: array must not be null
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i8)))
    (data $d "1337")
    (func (export "initData")
      (; array to init ;)       (ref.null $a)
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into data ;)  i32.const 0
      (; size=4 elements ;)     i32.const 4
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  }, WebAssembly.RuntimeError, /dereferencing null pointer/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #1
{
  let { initData } = wasmEvalText(`(module
    (memory 1)
    (type $a (array (mut i8)))
    (data $d (offset (i32.const 0)) "1337")
    (func (export "initData")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into data ;)  i32.const 0
      (; size=4 elements ;)     i32.const 4
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  }, WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #2
{
  let { initData } = wasmEvalText(`(module
    (memory 1)
    (type $a (array (mut i8)))
    (data $d (offset (i32.const 0)) "1337")
    (func (export "initData")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=4 into data ;)  i32.const 4
      (; size=0 elements ;)     i32.const 0
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #3
{
  let { initData } = wasmEvalText(`(module
    (memory 1)
    (type $a (array (mut i8)))
    (data $d (offset (i32.const 0)) "1337")
    (func (export "initData")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into data ;)  i32.const 0
      (; size=0 elements ;)     i32.const 0
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  initData();
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #4
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i8)))
    (data $d "1337")
    (func (export "initData")
      data.drop $d

      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into data ;)  i32.const 0
      (; size=4 elements ;)     i32.const 4
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #5
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i8)))
    (data $d "1337")
    (func (export "initData")
      data.drop $d

      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into data ;)  i32.const 0
      (; size=0 elements ;)     i32.const 0
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  initData();
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #6
{
  let { initData } = wasmEvalText(`(module
    (memory 1)
    (type $a (array (mut i8)))
    (data $d "1337")
    (func (export "initData")
      data.drop $d

      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=4 into array ;) i32.const 4
      (; offset=0 into data ;)  i32.const 0
      (; size=0 elements ;)     i32.const 0
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  initData();
}

// run: range to copy would require OOB read on data segment #1
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i8)))
    (data $d "1337")
    (func (export "initData")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=1 into data ;)  i32.const 1
      (; size=4 elements ;)     i32.const 4
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: range to copy would require OOB read on data segment #2
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i16)))
    (data $d "1337")
    (func (export "initData")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=1 into data ;)  i32.const 1
      (; size=2 elements ;)     i32.const 2 ;; still 4 bytes
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: range to copy would require OOB write on array #1
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i8)))
    (data $d "1337")
    (func (export "initData")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=1 into array ;) i32.const 1
      (; offset=0 into data ;)  i32.const 0
      (; size=4 elements ;)     i32.const 4
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: range to copy would require OOB write on array #2
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i16)))
    (data $d "1337")
    (func (export "initData")
      (; array to init ;)       (array.new_default $a (i32.const 2))
      (; offset=1 into array ;) i32.const 1
      (; offset=0 into data ;)  i32.const 0
      (; size=4 elements ;)     i32.const 2
      array.init_data $a $d
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  assertErrorMessage(() => {
    initData();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: resulting array is as expected
{
  let { initData } = wasmEvalText(`(module
    (type $a (array (mut i8)))
    (data $other "\\\\9")
    (data $d "1337")
    (func (export "initData") (result eqref)
      (local $arr (ref $a))
      (local.set $arr (array.new_default $a (i32.const 4)))

      (; array to init ;)       local.get $arr
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into data ;)  i32.const 0
      (; size=4 elements ;)     i32.const 4
      array.init_data $a $d

      local.get $arr
    )
    (func data.drop 0) ;; force write of data count section, see https://github.com/bytecodealliance/wasm-tools/pull/1194
  )`).exports;
  let arr = initData();
  assertEq(wasmGcArrayLength(arr), 4);
  assertEq(wasmGcReadField(arr, 0), 48+1);
  assertEq(wasmGcReadField(arr, 1), 48+3);
  assertEq(wasmGcReadField(arr, 2), 48+3);
  assertEq(wasmGcReadField(arr, 3), 48+7);
}

//////////////////////////////////////////////////////////////////////////////
//
// array.init_elem
/*
  validation:
    array-type imm-operand needs to be "in range"
    array-type imm-operand must refer to an array type
    array-type imm-operand must refer to an array of ref typed elements
    array-type imm-operand must be mutable
    destination elem type must be a supertype of src elem type
    segment index must be "in range"
  run:
    array must not be null
    if segment is "already used" (active, or passive that has subsequently
      been dropped), then only a zero length array can be created
    range to copy would require OOB read on element segment
    range to copy would require OOB write on array
    resulting array is as expected
*/

// validation: array-type imm-operand needs to be "in range"
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array (mut funcref)))
  (elem $e func $f1 $f2 $f3 $f4)
  (func $f1 (export "f1"))
  (func $f2 (export "f2"))
  (func $f3 (export "f3"))
  (func $f4 (export "f4"))
  (func (export "initElem")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into elem ;)  i32.const 0
    (; size=4 into elem ;)    i32.const 4
    array.init_elem 4 $e
  )
)`), WebAssembly.CompileError, /type index out of range/);

// validation: array-type imm-operand must refer to an array type
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (func (param i64) (result f64)))
  (elem $e func $f1 $f2 $f3 $f4)
  (func $f1 (export "f1"))
  (func $f2 (export "f2"))
  (func $f3 (export "f3"))
  (func $f4 (export "f4"))
  (func (export "initElem")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into elem ;)  i32.const 0
    (; size=4 into elem ;)    i32.const 4
    array.init_elem $a $e
  )
)`), WebAssembly.CompileError, /not an array type/);

// validation: array-type imm-operand must refer to an array of ref typed
//   elements
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array (mut f32)))
  (elem $e func $f1 $f2 $f3 $f4)
  (func $f1 (export "f1"))
  (func $f2 (export "f2"))
  (func $f3 (export "f3"))
  (func $f4 (export "f4"))
  (func (export "initElem")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into elem ;)  i32.const 0
    (; size=4 into elem ;)    i32.const 4
    array.init_elem $a $e
  )
)`), WebAssembly.CompileError, /element type is not a reftype/);

// validation: array-type imm-operand must be mutable
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array funcref))
  (elem $e func $f1 $f2 $f3 $f4)
  (func $f1 (export "f1"))
  (func $f2 (export "f2"))
  (func $f3 (export "f3"))
  (func $f4 (export "f4"))
  (func (export "initElem")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into elem ;)  i32.const 0
    (; size=4 into elem ;)    i32.const 4
    array.init_elem $a $e
  )
)`), WebAssembly.CompileError, /destination array is not mutable/);

// validation: destination elem type must be a supertype of src elem type
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array (mut eqref)))
  (elem $e func $f1)
  (func $f1 (export "f1"))
  ;; The copy here is from elem-seg-of-funcrefs to
  ;; array-of-eqrefs, which must fail, because funcref isn't
  ;; a subtype of eqref.
  (func (export "initElem")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into elem ;)  i32.const 0
    (; size=4 into elem ;)    i32.const 4
    array.init_elem $a $e
  )
)`), WebAssembly.CompileError, /incompatible element types/);

// validation: segment index must be "in range"
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array (mut funcref)))
  (elem $e func $f1 $f2 $f3 $f4)
  (func $f1 (export "f1"))
  (func $f2 (export "f2"))
  (func $f3 (export "f3"))
  (func $f4 (export "f4"))
  (func (export "initElem")
    (; array to init ;)       (array.new_default $a (i32.const 4))
    (; offset=0 into array ;) i32.const 0
    (; offset=0 into elem ;)  i32.const 0
    (; size=4 into elem ;)    i32.const 4
    array.init_elem $a 1 ;; 1 is the lowest invalid eseg index
  )
)`), WebAssembly.CompileError, /segment index is out of range/);

// run: array must not be null
{
  let { initElem } = wasmEvalText(`(module
    (type $a (array (mut funcref)))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      (; array to init ;)       (ref.null $a)
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into elem ;)  i32.const 0
      (; size=4 into elem ;)    i32.const 4
      array.init_elem $a $e
    )
  )`).exports;
  assertErrorMessage(() => {
    initElem();
  }, WebAssembly.RuntimeError, /dereferencing null pointer/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #1
{
  let { initElem } = wasmEvalText(`(module
    (table 4 funcref)
    (type $a (array (mut funcref)))
    (elem $e (offset (i32.const 0)) func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into elem ;)  i32.const 0
      (; size=4 into elem ;)    i32.const 4
      array.init_elem $a $e
    )
  )`).exports;
  assertErrorMessage(() => {
    initElem();
  }, WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #2
{
  let { initElem } = wasmEvalText(`(module
    (table 4 funcref)
    (type $a (array (mut funcref)))
    (elem $e (offset (i32.const 0)) func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=4 into elem ;)  i32.const 4
      (; size=0 into elem ;)    i32.const 0
      array.init_elem $a $e
    )
  )`).exports;
  assertErrorMessage(() => {
    initElem();
  }, WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length array can be created #3
{
  let { initElem } = wasmEvalText(`(module
    (table 4 funcref)
    (type $a (array (mut funcref)))
    (elem $e (offset (i32.const 0)) func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into elem ;)  i32.const 0
      (; size=0 into elem ;)    i32.const 0
      array.init_elem $a $e
    )
  )`).exports;
  initElem();
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #4
{
  let { initElem } = wasmEvalText(`(module
    (type $a (array (mut funcref)))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      elem.drop $e

      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into elem ;)  i32.const 0
      (; size=4 into elem ;)    i32.const 4
      array.init_elem $a $e
    )
  )`).exports;
  assertErrorMessage(() => {
    initElem();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #5
{
  let { initElem } = wasmEvalText(`(module
    (type $a (array (mut funcref)))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      elem.drop $e

      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into elem ;)  i32.const 0
      (; size=4 into elem ;)    i32.const 0
      array.init_elem $a $e
    )
  )`).exports;
  initElem();
}

// run: if segment is "already used" (active, or passive that has subsequently
//        been dropped), then only a zero length init can be performed #6
{
  let { initElem } = wasmEvalText(`(module
    (type $a (array (mut funcref)))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      elem.drop $e

      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 4
      (; offset=0 into elem ;)  i32.const 0
      (; size=4 into elem ;)    i32.const 0
      array.init_elem $a $e
    )
  )`).exports;
  initElem();
}

// run: range to copy would require OOB read on elem segment
{
  let { initElem } = wasmEvalText(`(module
    (type $a (array (mut funcref)))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into elem ;)  i32.const 1
      (; size=4 into elem ;)    i32.const 4
      array.init_elem $a $e
    )
  )`).exports;
  assertErrorMessage(() => {
    initElem();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: range to copy would require OOB write on array
{
  let { initElem } = wasmEvalText(`(module
    (type $a (array (mut funcref)))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem")
      (; array to init ;)       (array.new_default $a (i32.const 4))
      (; offset=0 into array ;) i32.const 1
      (; offset=0 into elem ;)  i32.const 0
      (; size=4 into elem ;)    i32.const 4
      array.init_elem $a $e
    )
  )`).exports;
  assertErrorMessage(() => {
    initElem();
  },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: resulting array is as expected
{
  let { initElem, f1, f2, f3, f4 } = wasmEvalText(`(module
    (type $a (array (mut funcref)))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 (export "f1"))
    (func $f2 (export "f2"))
    (func $f3 (export "f3"))
    (func $f4 (export "f4"))
    (func (export "initElem") (result eqref)
      (local $arr (ref $a))
      (local.set $arr (array.new_default $a (i32.const 4)))

      (; array to init ;)       local.get $arr
      (; offset=0 into array ;) i32.const 0
      (; offset=0 into elem ;)  i32.const 0
      (; size=4 into elem ;)    i32.const 4
      array.init_elem $a $e

      local.get $arr
    )
  )`).exports;
  let arr = initElem();
  assertEq(wasmGcArrayLength(arr), 4);
  assertEq(wasmGcReadField(arr, 0), f1);
  assertEq(wasmGcReadField(arr, 1), f2);
  assertEq(wasmGcReadField(arr, 2), f3);
  assertEq(wasmGcReadField(arr, 3), f4);
}

//////////////////////////////////////////////////////////////////////////////
//
// array.copy
/*
  validation:
    dest array must be mutable
    validation: src and dest arrays must have compatible element types
  run:
    check for OOB conditions on src/dest arrays for non-zero length copies
    check for OOB conditions on src/dest arrays for zero length copies
    check resulting arrays are as expected
    check that null src or dest array causes a trap
*/

// validation: dest array must be mutable
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i32))
    (func (param eqref)
      (array.copy $a $a (local.get 0) (i32.const 1)
                        (local.get 0) (i32.const 2) (i32.const 3))
    )
)
`), WebAssembly.CompileError, /array is not mutable/);

// validation: src and dest arrays must have compatible element types #1
assertErrorMessage(() => wasmEvalText(`(module
    (type $a32 (array (mut i32)))
    (type $a8  (array i8))
    (func (param eqref)
      (array.copy $a32 $a8 (local.get 0) (i32.const 1)
                           (local.get 0) (i32.const 2) (i32.const 3))
    )
)
`), WebAssembly.CompileError, /incompatible element types/);

// validation: src and dest arrays must have compatible element types #2
assertErrorMessage(() => wasmEvalText(`(module
    (type $a64 (array (mut i64)))
    (type $aER (array eqref))
    (func (param eqref)
      (array.copy $a64 $aER (local.get 0) (i32.const 1)
                            (local.get 0) (i32.const 2) (i32.const 3))
    )
)
`), WebAssembly.CompileError, /incompatible element types/);

// validation: src and dest arrays must have compatible element types #3
//
// We can only copy from a child (sub) reftype to a parent (super) reftype [or
// to the same reftype.]  Here, we have an array of eqref and an array of
// arrays.  An array is a child type of eqref, so it's invalid to try and copy
// eqrefs into the array of arrays.
assertErrorMessage(() => wasmEvalText(`(module
    (type $ty (array i32))
    (type $child  (array (mut (ref $ty))))
    (type $parent (array (mut eqref)))
    (func (param (ref null $child) (ref null $parent))
      ;; implied copy from parent to child -> not allowed
      (array.copy $child $parent (local.get 1) (i32.const 1)
                                 (local.get 0) (i32.const 2) (i32.const 3))
    )
)
`), WebAssembly.CompileError, /incompatible element types/);

// run: check for OOB conditions on src/dest arrays for non-zero length copies
// run: check for OOB conditions on src/dest arrays for zero length copies
// run: check resulting arrays are as expected
const ARRAY_COPY_TESTS = [
    // Format is:
    //   array element type
    //   corresponding value type
    //   source array
    //   first expected result
    //   second expected result
    //
    // Value-type cases
    [ 'i32', 'i32',
      [22, 33, 44, 55, 66, 77],
      [22, 55, 66, 77, 66, 77],
      [22, 33, 22, 33, 44, 77]
    ],
    [ 'i64', 'i64',
      [0x2022002220002n, 0x3033003330003n, 0x4044004440004n,
       0x5055005550005n, 0x6066006660006n, 0x7077007770007n],
      [0x2022002220002n, 0x5055005550005n, 0x6066006660006n,
       0x7077007770007n, 0x6066006660006n, 0x7077007770007n],
      [0x2022002220002n, 0x3033003330003n, 0x2022002220002n,
       0x3033003330003n, 0x4044004440004n, 0x7077007770007n]
    ],
    [ 'f32', 'f32',
      [22.0, 33.0, 44.0, 55.0, 66.0, 77.0],
      [22.0, 55.0, 66.0, 77.0, 66.0, 77.0],
      [22.0, 33.0, 22.0, 33.0, 44.0, 77.0]
    ],
    [ 'f64', 'f64',
      [22.0, 33.0, 44.0, 55.0, 66.0, 77.0],
      [22.0, 55.0, 66.0, 77.0, 66.0, 77.0],
      [22.0, 33.0, 22.0, 33.0, 44.0, 77.0]
    ],
    [ 'externref', 'externref',
      ['two', 'three', 'four',  'five',  'six', 'seven'],
      ['two',  'five',  'six', 'seven',  'six', 'seven'],
      ['two', 'three',  'two', 'three', 'four', 'seven']
    ],
    // non-Value-type cases
    [ 'i8', 'i32',
      [22, 33, 44, 55, 66, 77],
      [22, 55, 66, 77, 66, 77],
      [22, 33, 22, 33, 44, 77]
    ],
    [ 'i16', 'i32',
      [22, 33, 44, 55, 66, 77],
      [22, 55, 66, 77, 66, 77],
      [22, 33, 22, 33, 44, 77]
    ]
];

for (let [elemTy, valueTy, src, exp1, exp2] of ARRAY_COPY_TESTS) {
    let { arrayNew, arrayCopy } = wasmEvalText(
     `(module
        (type $arrTy (array (mut ${elemTy})))
        (func (export "arrayNew")
              (param ${valueTy} ${valueTy} ${valueTy}
                     ${valueTy} ${valueTy} ${valueTy})
              (result eqref)
          local.get 0
          local.get 1
          local.get 2
          local.get 3
          local.get 4
          local.get 5
          array.new_fixed $arrTy 6
        )
        (func (export "arrayCopy")
              (param eqref i32 eqref i32 i32)
          (array.copy $arrTy $arrTy
            (ref.cast (ref null $arrTy) local.get 0) (local.get 1)
            (ref.cast (ref null $arrTy) local.get 2) (local.get 3) (local.get 4)
          )
        )
      )`
    ).exports;

    assertEq(src.length, 6);
    assertEq(exp1.length, 6);
    assertEq(exp2.length, 6);

    function eqArrays(a1, a2) {
        function len(arr) {
            return Array.isArray(arr) ? arr.length : wasmGcArrayLength(arr);
        }
        function get(arr, i) {
            return Array.isArray(arr) ? arr[i] : wasmGcReadField(arr, i);
        }
        assertEq(len(a1), 6);
        assertEq(len(a2), 6);
        for (i = 0; i < 6; i++) {
            if (get(a1, i) !== get(a2, i))
                return false;
        }
        return true;
    }
    function show(who, arr) {
        print(who + ": " + arr[0] + " " + arr[1] + " " + arr[2] + " "
                         + arr[3] + " " + arr[4] + " " + arr[5] + " ");
    }

    // Check that "normal" copying gives expected results.
    let srcTO;
    srcTO = arrayNew(src[0], src[1], src[2], src[3], src[4], src[5]);
    arrayCopy(srcTO, 1, srcTO, 3, 3);
    assertEq(eqArrays(srcTO, exp1), true);

    srcTO = arrayNew(src[0], src[1], src[2], src[3], src[4], src[5]);
    arrayCopy(srcTO, 2, srcTO, 0, 3);
    assertEq(eqArrays(srcTO, exp2), true);

    // Check out-of-bounds conditions
    let exp1TO = arrayNew(exp1[0], exp1[1], exp1[2], exp1[3], exp1[4], exp1[5]);
    let exp2TO = arrayNew(exp2[0], exp2[1], exp2[2], exp2[3], exp2[4], exp2[5]);

    // dst overrun, wants to write [5, 6]
    assertErrorMessage(() => {
        arrayCopy(exp1TO, 5, exp2TO, 1, 2);
    },WebAssembly.RuntimeError, /index out of bounds/);

    // dst overrun, wants to write [7, 8]
    assertErrorMessage(() => {
        arrayCopy(exp1TO, 7, exp2TO, 1, 2);
    },WebAssembly.RuntimeError, /index out of bounds/);

    // dst zero-len overrun, wants to write no elements, but starting at 9
    assertErrorMessage(() => {
        arrayCopy(exp1TO, 9, exp2TO, 1, 0);
    },WebAssembly.RuntimeError, /index out of bounds/);

    // src overrun, wants to read [5, 6]
    assertErrorMessage(() => {
        arrayCopy(exp1TO, 1, exp2TO, 5, 2);
    },WebAssembly.RuntimeError, /index out of bounds/);

    // src overrun, wants to read [7, 8]
    assertErrorMessage(() => {
        arrayCopy(exp1TO, 1, exp2TO, 7, 2);
    },WebAssembly.RuntimeError, /index out of bounds/);

    // src zero-len overrun, wants to read no elements, but starting at 9
    assertErrorMessage(() => {
        arrayCopy(exp1TO, 1, exp2TO, 9, 0);
    },WebAssembly.RuntimeError, /index out of bounds/);
}

// run: check that null src or dest array causes a trap #1
{
  let { shouldTrap } = wasmEvalText(`(module
    (type $a (array (mut f32)))
    (func (export "shouldTrap")
      ref.null $a
      i32.const 1
      (array.new_fixed $a 3 (f32.const 1.23) (f32.const 4.56) (f32.const 7.89))
      i32.const 1
      i32.const 1
      array.copy $a $a
    )
  )`).exports;
  assertErrorMessage(() => {
    shouldTrap();
  }, WebAssembly.RuntimeError, /null/);
}

// run: check that null src or dest array causes a trap #2
{
  let { shouldTrap } = wasmEvalText(`(module
    (type $a (array (mut f32)))
    (func (export "shouldTrap")
      (array.new_fixed $a 3 (f32.const 1.23) (f32.const 4.56) (f32.const 7.89))
      i32.const 1
      ref.null $a
      i32.const 1
      i32.const 1
      array.copy $a $a
    )
  )`).exports;
  assertErrorMessage(() => {
    shouldTrap();
  }, WebAssembly.RuntimeError, /null/);
}

//////////////////////////////////////////////////////////////////////////////
//
// array.fill
/*
  validation:
    array must be mutable
    value must be compatible with array element type
  run:
    null array causes a trap
    OOB conditions on array for non-zero length copies
    OOB conditions on array for zero length copies
    resulting arrays are as expected (all types)
*/

// validation: array must be mutable
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array i32))
  (func
    (array.new_default $a (i32.const 8))
    i32.const 0
    i32.const 123
    i32.const 8
    array.fill $a
  )
)
`), WebAssembly.CompileError, /array is not mutable/);

// validation: value must be compatible with array element type #1
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array (mut i32)))
  (func
    (array.new_default $a (i32.const 8))
    i32.const 0
    i64.const 123
    i32.const 8
    array.fill $a
  )
)
`), WebAssembly.CompileError, /type mismatch/);

// validation: value must be compatible with array element type #2
assertErrorMessage(() => wasmEvalText(`(module
  (type $a (array (mut eqref)))
  (func
    (array.new_default $a (i32.const 8))
    i32.const 0
    ref.null any
    i32.const 8
    array.fill $a
  )
)
`), WebAssembly.CompileError, /type mismatch/);

// run: null array causes a trap
{
  const { arrayFill } = wasmEvalText(`(module
    (type $a (array (mut i32)))
    (func (export "arrayFill")
      ref.null $a
      i32.const 0
      i32.const 123
      i32.const 8
      array.fill $a
    )
  )`).exports;
  assertErrorMessage(() => {
    arrayFill();
  }, WebAssembly.RuntimeError, /dereferencing null pointer/);
}

// run: OOB conditions on array for non-zero length copies
{
  const { arrayFill } = wasmEvalText(`(module
    (type $a (array (mut i32)))
    (func (export "arrayFill")
      (array.new_default $a (i32.const 8))
      i32.const 1
      i32.const 123
      i32.const 8
      array.fill $a
    )
  )`).exports;
  assertErrorMessage(() => {
    arrayFill();
  }, WebAssembly.RuntimeError, /index out of bounds/);
}

// run: OOB conditions on array for zero length copies
{
  const { arrayFill } = wasmEvalText(`(module
    (type $a (array (mut i32)))
    (func (export "arrayFill")
      (array.new_default $a (i32.const 8))
      i32.const 8
      i32.const 123
      i32.const 0
      array.fill $a
    )
  )`).exports;
  arrayFill();
}

// run: arrays are as expected (all types)
{
  const TESTS = [
    { type: 'i8', val: 'i32.const 123', get: 'array.get_u', test: 'i32.eq' },
    { type: 'i16', val: 'i32.const 123', get: 'array.get_u', test: 'i32.eq' },
    { type: 'i32', val: 'i32.const 123', test: 'i32.eq' },
    { type: 'i64', val: 'i64.const 123', test: 'i64.eq' },
    { type: 'f32', val: 'f32.const 3.14', test: 'f32.eq' },
    { type: 'f64', val: 'f64.const 3.14', test: 'f64.eq' },
    { type: 'eqref', val: 'global.get 0', test: 'ref.eq' },
  ];
  if (wasmSimdEnabled()) {
    TESTS.push({ type: 'v128', val: 'v128.const i32x4 111 222 333 444', test: '(v128.xor) (i32.eq (v128.any_true) (i32.const 0))' });
  }

  for (const { type, val, get = 'array.get', test } of TESTS) {
    const { arrayFill, isDefault, isFilled } = wasmEvalText(`(module
      (type $a (array (mut ${type})))
      (type $s (struct))
      (global (ref $s) (struct.new_default $s))
      (func (export "arrayFill") (result (ref $a))
        (local $arr (ref $a))
        (local.set $arr (array.new_default $a (i32.const 4)))

        local.get $arr
        i32.const 1
        ${val}
        i32.const 2
        array.fill $a

        local.get $arr
      )
      (func (export "isDefault") (param (ref $a) i32) (result i32)
        (${get} $a (local.get 0) (local.get 1))
        (${get} $a (array.new_default $a (i32.const 1)) (i32.const 0))
        ${test}
      )
      (func (export "isFilled") (param (ref $a) i32) (result i32)
        (${get} $a (local.get 0) (local.get 1))
        ${val}
        ${test}
      )
    )`).exports;
    const arr = arrayFill();
    assertEq(isDefault(arr, 0), 1, `expected default value for ${type} but got filled`);
    assertEq(isFilled(arr, 1), 1, `expected filled value for ${type} but got default`);
    assertEq(isFilled(arr, 2), 1, `expected filled value for ${type} but got default`);
    assertEq(isDefault(arr, 3), 1, `expected default value for ${type} but got filled`);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Checks for requests for oversize arrays (more than MaxArrayPayloadBytes),
// where MaxArrayPayloadBytes == 1,987,654,321.

// array.new
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array i32))
    (func
        ;; request exactly 2,000,000,000 bytes
        (array.new $a (i32.const 0xABCD1234) (i32.const 500000000))
        drop
    )
    (start 0)
)
`), WebAssembly.RuntimeError, /too many array elements/);

// array.new_default
assertErrorMessage(() => wasmEvalText(`(module
    (type $a (array f64))
    (func
        ;; request exactly 2,000,000,000 bytes
        (array.new_default $a (i32.const 250000000))
        drop
    )
    (start 0)
)
`), WebAssembly.RuntimeError, /too many array elements/);

// array.new_fixed
// This is impossible to test because it would require to create, at a
// minimum, 1,987,654,321 (MaxArrayPayloadBytes) / 16 = 124.3 million
// values, if each value is a v128.  However, the max number of bytes per
// function is 7,654,321 (MaxFunctionBytes).  Even if it were possible to
// create a value using just one insn byte, there wouldn't be enough.

// array.new_data
// Similarly, impossible to test because the max data segment length is 1GB
// (1,073,741,824 bytes) (MaxDataSegmentLengthPages * PageSize), which is less
// than MaxArrayPayloadBytes.

// array.new_element
// Similarly, impossible to test because an element segment can contain at
// most 10,000,000 (MaxElemSegmentLength) entries.
