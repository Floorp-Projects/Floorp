// |jit-test| skip-if: !wasmGcEnabled() || getBuildConfiguration("simulator")

// This test is intended to test what was committed in
//
//  Bug 1817385 - wasm-gc: reduce cost of allocation and GC paths
//  and
//  Bug 1820120 - Manage Wasm{Array,Struct}Object OOL-storage-blocks
//                using a thread-private cache
//
// and in particular the latter.  The patches in these bugs reduce the cost of
// wasm-gc struct/array allocation and collection, in part by better
// integrating those objects with our generational GC facility.
//
// Existing tests do not cover all of those paths.  In particular they do not
// exercise both set-subtraction algorithms in Nursery::freeTrailerBlocks.
// This test does, though.
//
// The test first creates an "primary" array of 1500 elements.  Each element
// is a reference to a secondary array of between 1 and 50 int32s.  These
// secondary arrays have size chosen randomly, and the elements are also
// random.
//
// Then, elements of the primary array are replaced.  An index in the range 0
// .. N - 1 is randomly chosen, and the element there is replaced by a
// randomly-created secondary array.  This is repeated 500,000 times with
// N = 800.
//
// Finally, all of the above is repeated, but with N = 1200.
//
// As a result just over a million arrays and their trailer blocks, of various
// sizes, are allocated and deallocated.  With N = 800, in
// js::Nursery::freeTrailerBlocks, we end up with trailersRemovedUsed_ of
// around 800, so one of the set-subtraction algorithms is exercised.
// With N = 1200, the other is exercised.  It's not entirely clear why changing
// N causes trailersRemovedUsed_ to have more or less the same value during
// nursery collection, but the correlation does seem fairly robust.
//
// The test is skipped on the simulator because it takes too long to run, and
// triggers timeouts.

let t =
`(module

   ;; A simple pseudo-random number generator.
   ;; Produces numbers in the range 0 .. 2^16-1.
   (global $rngState
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

   ;; Create an array ("secondary array") containing random numbers, with a
   ;; size between 1 and 50, also randomly chosen.
   (func $createSecondaryArray (export "createSecondaryArray")
                               (result (ref $tArrayI32))
     (local $i i32)
     (local $nElems i32)
     (local $arr (ref $tArrayI32))
     (local.set $nElems (call $rand))
     (local.set $nElems (i32.rem_u (local.get $nElems) (i32.const 50)))
     (local.set $nElems (i32.add   (local.get $nElems) (i32.const 1)))
     (local.set $arr (array.new $tArrayI32 (i32.const 0) (local.get $nElems)))
     (loop $cont
       (array.set $tArrayI32 (local.get $arr) (local.get $i) (call $rand))
       (local.set $i (i32.add (local.get $i) (i32.const 1)))
       (br_if $cont (i32.lt_u (local.get $i) (local.get $nElems)))
     )
     (local.get $arr)
   )

   ;; Create an array (the "primary array") of 1500 elements of
   ;; type ref-of-tArrayI32.
   (func $createPrimaryArray (export "createPrimaryArray")
                             (result (ref $tArrayArrayI32))
     (local $i i32)
     (local $arrarr (ref $tArrayArrayI32))
     (local.set $arrarr (array.new $tArrayArrayI32 (ref.null $tArrayI32)
                                                   (i32.const 1500)))
     (loop $cont
       (array.set $tArrayArrayI32 (local.get $arrarr)
                                  (local.get $i) (call $createSecondaryArray))
       (local.set $i (i32.add (local.get $i) (i32.const 1)))
       (br_if $cont (i32.lt_u (local.get $i) (i32.const 1500)))
     )
     (local.get $arrarr)
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

let i = wasmEvalText(t);
let fns = i.exports;

assertEq(fns.churn(800), -575895114);
assertEq(fns.churn(1200), -1164697516);
