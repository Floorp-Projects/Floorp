// |jit-test| --wasm-function-references; --wasm-gc; skip-if: !wasmGcEnabled() || getBuildConfiguration("simulator")

// Tests GC references passed as arguments during return calls.
// Similar to js/src/jit-test/tests/wasm/gc/trailers-gc-stress.js

let base = wasmEvalText(`(module
    ;; A simple pseudo-random number generator.
    ;; Produces numbers in the range 0 .. 2^16-1.
    (global $rngState (export "rngState")
      (mut i32) (i32.const 1)
    )
    (func $rand (export "rand") (result i32)
      (local $t i32)
      ;; update $rngState
      (local.set $t (global.get $rngState))
      (local.set $t (i32.mul (local.get $t) (i32.const 1103515245)))
      (local.set $t (i32.add (local.get $t) (i32.const 12345)))
      (global.set $rngState (local.get $t))
      ;; pull 16 random bits out of it
      (local.set $t (i32.shr_u (local.get $t) (i32.const 15)))
      (local.set $t (i32.and (local.get $t) (i32.const 0xFFFF)))
      (local.get $t)
    )
 
    ;; Array types
    (type $tArrayI32      (array (mut i32)))  ;; "secondary array" above
    (type $tArrayArrayI32 (array (mut (ref null $tArrayI32)))) ;; "primary array"
 
    (func $createSecondaryArrayLoop (export "createSecondaryArrayLoop")
        (param $i i32) (param $arr (ref $tArrayI32))
        (result (ref $tArrayI32))
        (block $cont
            (br_if $cont (i32.ge_u (local.get $i) (array.len (local.get $arr))))
            (array.set $tArrayI32 (local.get $arr) (local.get $i) (call $rand))
            (return_call $createSecondaryArrayLoop
                (i32.add (local.get $i) (i32.const 1))
                (local.get $arr))
        )
        (local.get $arr)
    )
    ;; Create an array ("secondary array") containing random numbers, with a
    ;; size between 1 and 50, also randomly chosen.
    (func $createSecondaryArray (export "createSecondaryArray")
                                (result (ref $tArrayI32))
      (return_call $createSecondaryArrayLoop
        (i32.const 0)
        (array.new $tArrayI32
          (i32.const 0)
          (i32.add (i32.rem_u (call $rand) (i32.const 50)) (i32.const 1)))
      )
    )

    (func $createPrimaryArrayLoop (export "createPrimaryArrayLoop")
        (param $i i32) (param $arrarr (ref $tArrayArrayI32)) 
        (result (ref $tArrayArrayI32))
        (block $cont
            (br_if $cont (i32.ge_u (local.get $i) (array.len (local.get $arrarr))))
            (array.set $tArrayArrayI32 (local.get $arrarr)
                                       (local.get $i) (call $createSecondaryArray))
            (return_call $createPrimaryArrayLoop
                (i32.add (local.get $i) (i32.const 1))
                (local.get $arrarr))
        )
        (local.get $arrarr)
    )
)`);
let t =
`(module
    ;; Array types (the same as in the base)
    (type $tArrayI32      (array (mut i32)))  ;; "secondary array" above
    (type $tArrayArrayI32 (array (mut (ref null $tArrayI32)))) ;; "primary array"

    (import "" "rngState" (global $rngState (mut i32)))
    (import "" "rand" (func $rand (result i32)))
    (import "" "createSecondaryArrayLoop"
      (func $createSecondaryArrayLoop
        (param $i i32) (param $arr (ref $tArrayI32))
        (result (ref $tArrayI32))))
    (import "" "createPrimaryArrayLoop" 
      (func $createPrimaryArrayLoop
        (param $i i32) (param $arrarr (ref $tArrayArrayI32)) 
        (result (ref $tArrayArrayI32))))

    ;; Create an array ("secondary array") containing random numbers, with a
    ;; size between 1 and 50, also randomly chosen.
    ;; (Copy of the base one to create trampoline)
    (func $createSecondaryArray (export "createSecondaryArray")
                                (result (ref $tArrayI32))
      (return_call $createSecondaryArrayLoop
        (i32.const 0)
        (array.new $tArrayI32
          (i32.const 0)
          (i32.add (i32.rem_u (call $rand) (i32.const 50)) (i32.const 1)))
      )
    )

    ;; Create an array (the "primary array") of 1500 elements of
    ;; type ref-of-tArrayI32.
    (func $createPrimaryArray (export "createPrimaryArray")
                            (result (ref $tArrayArrayI32))
      (return_call $createPrimaryArrayLoop
        (i32.const 0)
        (array.new $tArrayArrayI32 (ref.null $tArrayI32) (i32.const 1500)))
    )
     
   ;; Use $createPrimaryArray to create an initial array.  Then randomly replace
   ;; elements for a while.
   (func $churn (export "churn") (param $thresh i32) (result i32)
     (local $i i32)
     (local $j i32)
     (local $finalSum i32)
     (local $arrarr (ref $tArrayArrayI32))
     (local $arr (ref null $tArrayI32))
     (local $arrLen i32)
     (local.set $arrarr (call $createPrimaryArray))
     ;; This loop iterates 500,000 times.  Each iteration, it chooses
     ;; a randomly element in $arrarr and replaces it with a new
     ;; random array of 32-bit ints.
     (loop $cont
       ;; make $j be a random number in 0 .. $thresh-1.
       ;; Then replace that index in $arrarr with a new random arrayI32.
       (local.set $j (i32.rem_u (call $rand) (local.get $thresh)))
       (array.set $tArrayArrayI32 (local.get $arrarr)
                                  (local.get $j) (call $createSecondaryArray))
       (local.set $i (i32.add (local.get $i) (i32.const 1)))
       (br_if $cont (i32.lt_u (local.get $i) (i32.const 500000)))
     )

     ;; Finally, compute a checksum by summing all the numbers
     ;; in all secondary arrays.  This simply assumes that all of the refs to
     ;; secondary arrays are non-null, which isn't per-se guaranteed by the
     ;; previous loop, but it works in this case because the RNG
     ;; produces each index value to overwrite at least once.
     (local.set $finalSum (i32.const 0))
     (local.set $i (i32.const 0)) ;; loop var for the outer loop
     (loop $outer
       ;; body of outer loop
       ;; $arr = $arrarr[i]
       (local.set $arr (array.get $tArrayArrayI32 (local.get $arrarr)
                       (local.get $i)))
       ;; iterate over $arr
       (local.set $arrLen (array.len (local.get $arr)))
       (local.set $j (i32.const 0)) ;; loop var for the inner loop
       (loop $inner
         ;; body of inner loop
         (local.set $finalSum
                    (i32.rotl (local.get $finalSum) (i32.const 1)))
         (local.set $finalSum
                    (i32.xor (local.get $finalSum)
                             (array.get $tArrayI32 (local.get $arr)
                                                   (local.get $j))))
         ;; loop control for the inner loop
         (local.set $j (i32.add (local.get $j) (i32.const 1)))
         (br_if $inner (i32.lt_u (local.get $j) (local.get $arrLen)))
       )
       ;; loop control for the outer loop
       (local.set $i (i32.add (local.get $i) (i32.const 1)))
       (br_if $outer (i32.lt_u (local.get $i) (i32.const 1500)))
     )

     ;; finally, roll in the final value of the RNG state
     (i32.xor (local.get $finalSum) (global.get $rngState))
   )
)`;

let i = wasmEvalText(t, {"": base.exports,});
let fns = i.exports;

assertEq(fns.churn(800), -575895114);
assertEq(fns.churn(1200), -1164697516);

wasmValidateText(`(module
  (rec
    (type $s1 (sub (struct (field i32))))
    (type $s2 (sub $s1 (struct (field i32 f32))))
  )
  (func (result (ref $s2))
    struct.new_default $s2
  )
  (func (export "f") (result (ref $s1))
    return_call 0
  )
)`);

wasmFailValidateText(`(module
  (rec
    (type $s1 (sub (struct (field i32))))
    (type $s2 (sub $s1 (struct (field i32 f32))))
  )
  (func (result (ref $s1))
    struct.new_default $s1
  )
  (func (export "f") (result (ref $s2))
    return_call 0
  )
)`, /type mismatch/);

wasmValidateText(`(module
  (rec
    (type $s1 (sub (struct (field i32))))
    (type $s2 (sub $s1 (struct (field i32 f32))))
  )
  (type $t (func (result (ref $s2))))
  (func (export "f") (param (ref $t)) (result (ref $s1))
    local.get 0
    return_call_ref $t
  )
)`);

wasmFailValidateText(`(module
  (rec
    (type $s1 (sub (struct (field i32))))
    (type $s2 (sub $s1 (struct (field i32 f32))))
  )
  (type $t (func (result (ref $s1))))
  (func (export "f") (param (ref $t)) (result (ref $s2))
    local.get 0
    return_call_ref $t
  )
)`, /type mismatch/);
