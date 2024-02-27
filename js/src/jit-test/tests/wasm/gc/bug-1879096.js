// |jit-test| test-also=--setpref=wasm_test_serialization; skip-if: !wasmGcEnabled()

// Conditional branch instructions need to rewrite their stack types according
// to the destination label types. This loses information but is mandated by
// the spec.

// br_if
wasmFailValidateText(`(module
  (func (result anyref)
    ref.null array      ;; stack: [arrayref]
    ref.null struct     ;; stack: [arrayref structref]
    i32.const 0         ;; stack: [arrayref structref i32]
    br_if 0             ;; stack: [arrayref anyref]
    ref.eq              ;; should fail (anyref is not eq)
    unreachable
  )
)`, /type mismatch: expression has type anyref but expected eqref/);

// br_on_null
wasmFailValidateText(`(module
  (func (param externref) (result anyref)
    ref.null array      ;; stack: [arrayref]
    local.get 0         ;; stack: [arrayref externref]
    br_on_null 0        ;; stack: [anyref (ref extern)]
    drop                ;; stack: [anyref]
    array.len           ;; should fail
    unreachable
  )
)`, /type mismatch: expression has type anyref but expected arrayref/);

// br_on_non_null
wasmFailValidateText(`(module
  (func (param externref) (result anyref (ref extern))
    ref.null array      ;; stack: [arrayref]
    ref.null struct     ;; stack: [arrayref structref]
    local.get 0         ;; stack: [arrayref structref externref]
    br_on_non_null 0    ;; stack: [arrayref anyref]
    ref.eq              ;; should fail (anyref is not eq)
    unreachable
  )
)`, /type mismatch: expression has type anyref but expected eqref/);

// br_on_cast
wasmFailValidateText(`(module
  (type $s (struct))
  (func (result anyref (ref $s))
    ref.null array                    ;; stack: [arrayref]
    ref.null struct                   ;; stack: [arrayref structref]
    br_on_cast 0 structref (ref $s)   ;; stack: [anyref structref]
    ref.eq                            ;; should fail (anyref is not eq)
    unreachable
  )
)`, /type mismatch: expression has type anyref but expected eqref/);

// br_on_cast_fail
wasmFailValidateText(`(module
  (type $s (struct))
  (func (result anyref anyref)
    ref.null array                        ;; stack: [arrayref]
    ref.null struct                       ;; stack: [arrayref structref]
    br_on_cast_fail 0 structref (ref $s)  ;; stack: [anyref (ref $s)]
    ref.eq                                ;; should fail (anyref is not eq)
    unreachable
  )
)`, /type mismatch: expression has type anyref but expected eqref/);
