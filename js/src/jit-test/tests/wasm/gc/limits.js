// |jit-test| skip-if: !wasmGcEnabled() || getBuildConfiguration("tsan")

// This test has a timeout on TSAN configurations due to the large
// allocations.

// Limit of 1 million recursion groups
wasmValidateText(`(module
    ${`(rec (type (func)))`.repeat(1_000_000)}
  )`);
wasmFailValidateText(`(module
    ${`(rec (type (func)))`.repeat(1_000_001)}
  )`, /too many/);

// Limit of 1 million types (across all recursion groups)
wasmValidateText(`(module
    (rec ${`(type (func))`.repeat(1_000_000)})
  )`);
wasmValidateText(`(module
    (rec ${`(type (func))`.repeat(500_000)})
    (rec ${`(type (func))`.repeat(500_000)})
  )`);
wasmFailValidateText(`(module
    (rec ${`(type (func))`.repeat(1_000_001)})
  )`, /too many/);
wasmFailValidateText(`(module
    (rec ${`(type (func))`.repeat(500_000)})
    (rec ${`(type (func))`.repeat(500_001)})
  )`, /too many/);

// Limit of subtyping hierarchy 63 deep
function testSubtypingModule(depth) {
  let types = '(type (sub (func)))';
  for (let i = 1; i <= depth; i++) {
    types += `(type (sub ${i - 1} (func)))`;
  }
  return `(module
      ${types}
    )`;
}
wasmValidateText(testSubtypingModule(63));
wasmFailValidateText(testSubtypingModule(64), /too deep/);

// Limit of 10_000 struct fields
wasmFailValidateText(`(module
  (type (struct ${'(field i64)'.repeat(10_001)}))
)`, /too many/);

{
  let {makeLargeStructDefault, makeLargeStruct} = wasmEvalText(`(module
    (type $s (struct ${'(field i64)'.repeat(10_000)}))
    (func (export "makeLargeStructDefault") (result anyref)
      struct.new_default $s
    )
    (func (export "makeLargeStruct") (result anyref)
      ${'i64.const 0 '.repeat(10_000)}
      struct.new $s
    )
  )`).exports;
  let largeStructDefault = makeLargeStructDefault();
  let largeStruct = makeLargeStruct();
}

// array.new_fixed has limit of 10_000 operands
wasmFailValidateText(`(module
  (type $a (array i32))
  (func
    array.new_fixed $a 10001
  )
)`, /too many/);
