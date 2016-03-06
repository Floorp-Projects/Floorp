load(libdir + "wasm.js");

// ----------------------------------------------------------------------------
// if_else

// Condition is an int32
assertErrorMessage(() => wasmEvalText('(module (func (local f32) (if (get_local 0) (i32.const 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (local f32) (if_else (get_local 0) (i32.const 1) (i32.const 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (local f64) (if_else (get_local 0) (i32.const 1) (i32.const 0))))'), TypeError, mismatchError("f64", "i32"));
wasmEvalText('(module (func (local i32) (if (get_local 0) (nop))) (export "" 0))');
wasmEvalText('(module (func (local i32) (if_else (get_local 0) (nop) (nop))) (export "" 0))');

// Expression values types are consistent
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (local f32) (if_else (i32.const 42) (get_local 0) (i32.const 0))))'), TypeError, mismatchError("void", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (local f64) (if_else (i32.const 42) (i32.const 0) (get_local 0))))'), TypeError, mismatchError("void", "i32"));
assertEq(wasmEvalText('(module (func (result i32) (if_else (i32.const 42) (i32.const 1) (i32.const 2))) (export "" 0))')(), 1);
assertEq(wasmEvalText('(module (func (result i32) (if_else (i32.const 0) (i32.const 1) (i32.const 2))) (export "" 0))')(), 2);

// If we don't yield, sub expressions types don't have to match
assertEq(wasmEvalText('(module (func (if_else (i32.const 42) (i32.const 1) (i32.const 0))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (param f32) (if_else (i32.const 42) (i32.const 1) (get_local 0))) (export "" 0))')(13.37), undefined);

// Sub-expression values are returned
assertEq(wasmEvalText(`(module
    (func
        (result i32)
        (if_else
            (i32.const 42)
            (block
                (
                    if_else
                    (block
                        (i32.const 3)
                        (i32.const 5)
                        (i32.const 0)
                    )
                    (i32.const 1)
                    (i32.const 2)
                )
            )
            (i32.const 0)
        )
    )
    (export "" 0)
)`)(), 2);

// The if (resp. else) branch is taken iff the condition is true (resp. false)
counter = 0;
var imports = { inc() { counter++ } };
assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (result i32)
        (if_else
            (i32.const 42)
            (i32.const 1)
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), 1);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (result i32)
        (if_else
            (i32.const 0)
            (call_import 0)
            (i32.const 1)
        )
    )
    (export "" 0)
)`, imports)(), 1);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (if
            (i32.const 0)
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), undefined);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (if
            (i32.const 1)
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), undefined);
assertEq(counter, 1);

// One can chain if_else with if/if_else
counter = 0;
assertEq(wasmEvalText(`(module
    (import "inc" "" (result i32))
    (func
        (result i32)
        (if_else
            (i32.const 1)
            (if_else
                (i32.const 2)
                (if_else
                    (i32.const 3)
                    (if_else
                        (i32.const 0)
                        (call_import 0)
                        (i32.const 42)
                    )
                    (call_import 0)
                )
                (call_import 0)
            )
            (call_import 0)
        )
    )
    (export "" 0)
)`, imports)(), 42);
assertEq(counter, 0);

// "if" doesn't return an expression value
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (if (i32.const 42) (i32.const 0))))'), TypeError, mismatchError("void", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (if_else (i32.const 1) (i32.const 0) (if (i32.const 1) (i32.const 1)))))'), TypeError, mismatchError("void", "i32"));
wasmEvalText('(module (func (if_else (i32.const 1) (i32.const 0) (if (i32.const 1) (i32.const 1)))))');

// ----------------------------------------------------------------------------
// return

assertEq(wasmEvalText('(module (func (return)) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (result i32) (return (i32.const 1))) (export "" 0))')(), 1);
assertEq(wasmEvalText('(module (func (if (return) (i32.const 0))) (export "" 0))')(), undefined);
assertErrorMessage(() => wasmEvalText('(module (func (result f32) (return (i32.const 1))) (export "" 0))'), TypeError, mismatchError("i32", "f32"));
assertThrowsInstanceOf(() => wasmEvalText('(module (func (result i32) (return)) (export "" 0))'), TypeError);

// TODO: Reenable when syntactic arities are added for returns
//assertThrowsInstanceOf(() => wasmEvalText('(module (func (return (i32.const 1))) (export "" 0))'), TypeError);

// TODO: convert these to wasmEval and assert some results once they are implemented

// ----------------------------------------------------------------------------
// br / br_if

assertErrorMessage(() => wasmEvalText('(module (func (result i32) (block (br 0))) (export "" 0))'), TypeError, mismatchError("void", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (block (br_if 0 (i32.const 0)))) (export "" 0))'), TypeError, mismatchError("void", "i32"));

const DEPTH_OUT_OF_BOUNDS = /branch depth exceeds current nesting level/;

assertErrorMessage(() => wasmEvalText('(module (func (br 0)))'), TypeError, DEPTH_OUT_OF_BOUNDS);
assertErrorMessage(() => wasmEvalText('(module (func (block (br 1))))'), TypeError, DEPTH_OUT_OF_BOUNDS);
assertErrorMessage(() => wasmEvalText('(module (func (loop (br 2))))'), TypeError, DEPTH_OUT_OF_BOUNDS);

assertErrorMessage(() => wasmEvalText('(module (func (br_if 0 (i32.const 0))))'), TypeError, DEPTH_OUT_OF_BOUNDS);
assertErrorMessage(() => wasmEvalText('(module (func (block (br_if 1 (i32.const 0)))))'), TypeError, DEPTH_OUT_OF_BOUNDS);
assertErrorMessage(() => wasmEvalText('(module (func (loop (br_if 2 (i32.const 0)))))'), TypeError, DEPTH_OUT_OF_BOUNDS);

assertErrorMessage(() => wasmEvalText(`(module (func (result i32)
  (block
    (if_else
      (br 0)
      (i32.const 0)
      (i32.const 2)
    )
  )
) (export "" 0))`), TypeError, mismatchError("void", "i32"));

assertEq(wasmEvalText(`(module (func (block $out (br_if $out (br 0)))) (export "" 0))`)(), undefined);

assertEq(wasmEvalText('(module (func (block (br 0))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (block $l (br $l))) (export "" 0))')(), undefined);

assertEq(wasmEvalText('(module (func (block (block (br 1)))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (block $l (block (br $l)))) (export "" 0))')(), undefined);

assertEq(wasmEvalText('(module (func (block $l (block $m (br $l)))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (block $l (block $m (br $m)))) (export "" 0))')(), undefined);

assertEq(wasmEvalText(`(module (func (result i32)
  (block
    (br 0)
    (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "" 0))`)(), 1);

assertEq(wasmEvalText(`(module (func (result i32)
  (block
    (block
      (br 0)
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "" 0))`)(), 1);

assertEq(wasmEvalText(`(module (func (result i32)
  (block $outer
    (block $inner
      (br $inner)
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "" 0))`)(), 1);

// TODO enable once bug 1253572 is fixed.
/*
var notcalled = false;
var called = false;
var imports = {
    notcalled() {notcalled = true},
    called() {called = true}
};
assertEq(wasmEvalText(`(module (import "inc" "") (func
  (block
    (return (br 0))
    (call_import 0)
  )
  (call_import 1)
) (export "" 0))`)(imports), undefined);
assertEq(notcalled, false);
assertEq(called, true);

assertEq(wasmEvalText(`(module (func
  (block
    (i32.add
      (i32.const 0)
      (return (br 0))
    )
  )
  (return)
) (export "" 0))`)(), 1);
*/

assertEq(wasmEvalText(`(module (func (result i32)
  (block
    (if_else
      (i32.const 1)
      (br 0)
      (return (i32.const 0))
    )
  )
  (return (i32.const 1))
) (export "" 0))`)(), 1);

assertEq(wasmEvalText('(module (func (block (br_if 0 (i32.const 1)))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (block (br_if 0 (i32.const 0)))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (block $l (br_if $l (i32.const 1)))) (export "" 0))')(), undefined);

var isNonZero = wasmEvalText(`(module (func (result i32) (param i32)
  (block
    (br_if 0 (get_local 0))
    (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "" 0))`);

assertEq(isNonZero(0), 0);
assertEq(isNonZero(1), 1);
assertEq(isNonZero(-1), 1);

// ----------------------------------------------------------------------------
// loop

assertErrorMessage(() => wasmEvalText('(module (func (loop (br 2))))'), TypeError, DEPTH_OUT_OF_BOUNDS);

assertEq(wasmEvalText('(module (func (loop)) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (result i32) (loop (i32.const 2)) (i32.const 1)) (export "" 0))')(), 1);

assertEq(wasmEvalText('(module (func (loop (br 1))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (loop $a (br $a))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (loop $a $b (br $a))) (export "" 0))')(), undefined);

assertEq(wasmEvalText(`(module (func (result i32) (local i32)
  (loop
    $break $continue
    (if
      (i32.gt_u (get_local 0) (i32.const 5))
      (br $break)
    )
    (set_local 0 (i32.add (get_local 0) (i32.const 1)))
    (br $continue)
  )
  (return (get_local 0))
) (export "" 0))`)(), 6);

assertEq(wasmEvalText(`(module (func (result i32) (local i32)
  (loop
    $break $continue
    (br_if
      $break
      (i32.gt_u (get_local 0) (i32.const 5))
    )
    (set_local 0 (i32.add (get_local 0) (i32.const 1)))
    (br $continue)
  )
  (return (get_local 0))
) (export "" 0))`)(), 6);

assertEq(wasmEvalText(`(module (func (result i32) (local i32)
  (loop
    $break $continue
    (set_local 0 (i32.add (get_local 0) (i32.const 1)))
    (br_if
      $continue
      (i32.le_u (get_local 0) (i32.const 5))
    )
  )
  (return (get_local 0))
) (export "" 0))`)(), 6);

assertEq(wasmEvalText(`(module (func (result i32) (local i32)
  (loop
    $break $continue
    (br_if
      $break
      (i32.gt_u (get_local 0) (i32.const 5))
    )
    (set_local 0 (i32.add (get_local 0) (i32.const 1)))
    (loop
      (br $continue)
    )
    (return (i32.const 42))
  )
  (return (get_local 0))
) (export "" 0))`)(), 6);

assertEq(wasmEvalText(`(module (func (result i32) (local i32)
  (loop
    $break $continue
    (set_local 0 (i32.add (get_local 0) (i32.const 1)))
    (loop
      (br_if
        $continue
        (i32.le_u (get_local 0) (i32.const 5))
      )
    )
    (br $break)
  )
  (return (get_local 0))
) (export "" 0))`)(), 6);

// ----------------------------------------------------------------------------
// br_table

assertErrorMessage(() => wasmEvalText('(module (func (br_table (i32.const 0) (table) (br 0))))'), TypeError, DEPTH_OUT_OF_BOUNDS);

assertErrorMessage(() => wasmEvalText('(module (func (block (br_table (i32.const 0) (table (br 2)) (br 0)))))'), TypeError, DEPTH_OUT_OF_BOUNDS);

assertErrorMessage(() => wasmEvalText('(module (func (block (br_table (f32.const 0) (table) (br 0)))))'), TypeError, mismatchError("f32", "i32"));

assertEq(wasmEvalText(`(module (func (result i32) (param i32)
  (block $default
   (br_table (get_local 0) (table) (br $default))
   (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "" 0))`)(), 1);

assertEq(wasmEvalText(`(module (func (result i32) (param i32)
  (block $default
   (br_table (return (i32.const 1)) (table) (br $default))
   (return (i32.const 0))
  )
  (return (i32.const 2))
) (export "" 0))`)(), 1);

assertEq(wasmEvalText(`(module (func (result i32) (param i32)
  (block $outer
   (block $inner
    (br_table (get_local 0) (table) (br $inner))
    (return (i32.const 0))
   )
   (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "" 0))`)(), 1);

var f = wasmEvalText(`(module (func (result i32) (param i32)
  (block $0
   (block $1
    (block $2
     (block $default
      (br_table
       (get_local 0)
       (table (br $0) (br $1) (br $2))
       (br $default)
      )
     )
     (return (i32.const -1))
    )
    (return (i32.const 2))
   )
  )
  (return (i32.const 0))
) (export "" 0))`);

assertEq(f(-2), -1);
assertEq(f(-1), -1);
assertEq(f(0), 0);
assertEq(f(1), 0);
assertEq(f(2), 2);
assertEq(f(3), -1);

