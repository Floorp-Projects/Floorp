// |jit-test| skip-if: !wasmSimdEnabled()

wasmFailValidateText(`
(module
  (func
    (select
     (v128.const i32x4 1 2 3 4)
     (v128.const i32x4 4 3 2 1)
     (i32.const 0)
    )
  )
)
`, /untyped select/);

wasmAssert(`
(module
 (func $f (param i32) (result v128)
  (select (result v128)
   (v128.const i32x4 1 2 3 4)
   (v128.const i32x4 4 3 2 1)
   (local.get 0)
  )
 )
 (export "" (func 0))
)`, [
    { type: 'v128', func: '$f', args: ['i32.const  0'], expected: 'i32x4 4 3 2 1' },
    { type: 'v128', func: '$f', args: ['i32.const  1'], expected: 'i32x4 1 2 3 4' },
    { type: 'v128', func: '$f', args: ['i32.const -1'], expected: 'i32x4 1 2 3 4' },
], {});
