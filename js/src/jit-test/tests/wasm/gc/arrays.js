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
      rtt.canon $a
      array.new_with_rtt $a
    )

    (; new T[0] ;)
    (func (export "createDefault") (param i32) (result eqref)
      local.get 0
      rtt.canon $a
      array.new_default_with_rtt $a
    )

    (; 0[1] ;)
    (func (export "get") (param eqref i32) (result ${valtype})
      local.get 0
      rtt.canon $a
      ref.cast eq $a
      local.get 1
      array.get $a
    )

    (; 0[1] = 2 ;)
    (func (export "set") (param eqref i32 ${valtype})
      local.get 0
      rtt.canon $a
      ref.cast eq $a
      local.get 1
      local.get 2
      array.set $a
    )

    (; len(a) ;)
    (func (export "len") (param eqref) (result i32)
      local.get 0
      rtt.canon $a
      ref.cast eq $a
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
      rtt.canon $a
      array.new_with_rtt $a
    )

    (; 0[1] ;)
    (func (export "getS") (param eqref i32) (result i32)
      local.get 0
      rtt.canon $a
      ref.cast eq $a
      local.get 1
      array.get_s $a
    )

    (; 0[1] ;)
    (func (export "getU") (param eqref i32) (result i32)
      local.get 0
      rtt.canon $a
      ref.cast eq $a
      local.get 1
      array.get_u $a
    )

    (; 0[1] = 2 ;)
    (func (export "set") (param eqref i32 i32)
      local.get 0
      rtt.canon $a
      ref.cast eq $a
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
        (array.new_with_rtt $a
          i32.const 0xff
          i32.const 10
          rtt.canon $a)
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
