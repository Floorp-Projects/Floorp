// |jit-test| --wasm-gc; skip-if: !wasmGcEnabled()

// Validates if imported globals are accounted for in init expressions.

var ins = wasmEvalText(`(module
    (import "" "d" (global $g0 i32))
    (global $g i32 (i32.const 1))
    (global $g2 (mut i32) (global.get $g))
    (global $g3 (mut i32) (global.get $g0))
    (func (export "test1") (result i32) global.get $g2)
    (func (export "test2") (result i32) global.get $g3)
)`, {"": {d: 2},});
assertEq(ins.exports.test1(), 1);
assertEq(ins.exports.test2(), 2);

wasmFailValidateText(`(module
    (import "" "d" (global $g0 (mut i32)))
    (global $g (mut i32)  (global.get $g0))
)`, /must reference a global immutable import/);

wasmFailValidateText(`(module
    (import "" "d" (global $g0 i32))
    (global $g i32 (global.get $g))
)`, /global\.get index out of range/);

// Other tests from the bug.

wasmValidateText(`(module
    (import "xx" "d" (global $g0 i32))
    (global $int i32 (i32.const 251))
    (global $tbl2 (ref i31) (ref.i31
      (global.get $int)
    ))
)`);
wasmValidateText(`(module
    (import "xx" "d" (global $g0 i32))
    (type $block (array (ref eq)))
    (global $len i32 (i32.const 256))
    (global $tbl (ref $block) (array.new $block
     (ref.i31
      (i32.const 0)
     )
     (global.get $len)
    ))
)`);
