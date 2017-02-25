const RuntimeError = WebAssembly.RuntimeError;

// ----------------------------------------------------------------------------
// if

// Condition is an int32
wasmFailValidateText('(module (func (local f32) (if (get_local 0) (i32.const 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (local f32) (if (get_local 0) (i32.const 1) (i32.const 0))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (local f64) (if (get_local 0) (i32.const 1) (i32.const 0))))', mismatchError("f64", "i32"));
wasmEvalText('(module (func (local i32) (if (get_local 0) (nop))) (export "" 0))');
wasmEvalText('(module (func (local i32) (if (get_local 0) (nop) (nop))) (export "" 0))');

// Expression values types are consistent
wasmFailValidateText('(module (func (result i32) (local f32) (if f32 (i32.const 42) (get_local 0) (i32.const 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (result i32) (local f64) (if i32 (i32.const 42) (i32.const 0) (get_local 0))))', mismatchError("f64", "i32"));
assertEq(wasmEvalText('(module (func (result i32) (if i32 (i32.const 42) (i32.const 1) (i32.const 2))) (export "" 0))').exports[""](), 1);
assertEq(wasmEvalText('(module (func (result i32) (if i32 (i32.const 0) (i32.const 1) (i32.const 2))) (export "" 0))').exports[""](), 2);

// Even if we don't yield, sub expressions types still have to match.
wasmFailValidateText('(module (func (param f32) (if i32 (i32.const 42) (i32.const 1) (get_local 0))) (export "" 0))', mismatchError('f32', 'i32'));
wasmFailValidateText('(module (func (if i32 (i32.const 42) (i32.const 1) (i32.const 0))) (export "" 0))', /unused values not explicitly dropped by end of block/);
wasmFullPass('(module (func (drop (if i32 (i32.const 42) (i32.const 1) (i32.const 0)))) (export "run" 0))', undefined);
wasmFullPass('(module (func (param f32) (if (i32.const 42) (drop (i32.const 1)) (drop (get_local 0)))) (export "run" 0))', undefined, {}, 13.37);

// Sub-expression values are returned
wasmFullPass(`(module
    (func
        (result i32)
        (if i32
            (i32.const 42)
            (block i32
                (
                    if i32
                    (block i32
                        (drop (i32.const 3))
                        (drop (i32.const 5))
                        (i32.const 0)
                    )
                    (i32.const 1)
                    (i32.const 2)
                )
            )
            (i32.const 0)
        )
    )
    (export "run" 0)
)`, 2);

// The if (resp. else) branch is taken iff the condition is true (resp. false)
counter = 0;
var imports = { "":{inc() { counter++ }} };
wasmFullPass(`(module
    (import "" "inc" (result i32))
    (func
        (result i32)
        (if i32
            (i32.const 42)
            (i32.const 1)
            (call 0)
        )
    )
    (export "run" 1)
)`, 1, imports);
assertEq(counter, 0);

wasmFullPass(`(module
    (import "" "inc" (result i32))
    (func
        (result i32)
        (if i32
            (i32.const 0)
            (call 0)
            (i32.const 1)
        )
    )
    (export "run" 1)
)`, 1, imports);
assertEq(counter, 0);

wasmFullPass(`(module
    (import "" "inc" (result i32))
    (func
        (if
            (i32.const 0)
            (drop (call 0))
        )
    )
    (export "run" 1)
)`, undefined, imports);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "" "inc" (result i32))
    (func
        (if
            (i32.const 1)
            (drop (call 0))
        )
    )
    (export "" 1)
)`, imports).exports[""](), undefined);
assertEq(counter, 1);

wasmFullPass(`(module
    (func
        (result i32)
        (if i32
            (i32.const 0)
            (br 0 (i32.const 0))
            (br 0 (i32.const 1))
        )
    )
    (export "run" 0)
)`, 1);
assertEq(counter, 1);

wasmFullPass(`(module
    (func
        (if
            (i32.const 1)
            (br 0)
        )
    )
    (export "run" 0)
)`, undefined);
assertEq(counter, 1);

// One can chain if with if/if
counter = 0;
wasmFullPass(`(module
    (import "" "inc" (result i32))
    (func
        (result i32)
        (if i32
            (i32.const 1)
            (if i32
                (i32.const 2)
                (if i32
                    (i32.const 3)
                    (if i32
                        (i32.const 0)
                        (call 0)
                        (i32.const 42)
                    )
                    (call 0)
                )
                (call 0)
            )
            (call 0)
        )
    )
    (export "run" 1)
)`, 42, imports);
assertEq(counter, 0);

// "if" doesn't return an expression value
wasmFailValidateText('(module (func (result i32) (if i32 (i32.const 42) (i32.const 0))))', /if without else with a result value/);
wasmFailValidateText('(module (func (result i32) (if i32 (i32.const 42) (drop (i32.const 0)))))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (if i32 (i32.const 1) (i32.const 0) (if i32 (i32.const 1) (i32.const 1)))))', /if without else with a result value/);
wasmFailValidateText('(module (func (result i32) (if i32 (i32.const 1) (drop (i32.const 0)) (if (i32.const 1) (drop (i32.const 1))))))', emptyStackError);
wasmFailValidateText('(module (func (if i32 (i32.const 1) (i32.const 0) (if i32 (i32.const 1) (i32.const 1)))))', /if without else with a result value/);
wasmFailValidateText('(module (func (if i32 (i32.const 1) (i32.const 0) (if (i32.const 1) (drop (i32.const 1))))))', emptyStackError);
wasmFailValidateText('(module (func (if (i32.const 1) (drop (i32.const 0)) (if i32 (i32.const 1) (i32.const 1)))))', /if without else with a result value/);
wasmEvalText('(module (func (if (i32.const 1) (drop (i32.const 0)) (if (i32.const 1) (drop (i32.const 1))))))');

// ----------------------------------------------------------------------------
// return

wasmFullPass('(module (func (return)) (export "run" 0))', undefined);
wasmFullPass('(module (func (result i32) (return (i32.const 1))) (export "run" 0))', 1);
wasmFailValidateText('(module (func (if (return) (i32.const 0))) (export "run" 0))', unusedValuesError);
wasmFailValidateText('(module (func (result i32) (return)) (export "" 0))', emptyStackError);
wasmFullPass('(module (func (return (i32.const 1))) (export "run" 0))', undefined);
wasmFailValidateText('(module (func (result f32) (return (i32.const 1))) (export "" 0))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (result i32) (return)) (export "" 0))', emptyStackError);

// ----------------------------------------------------------------------------
// br / br_if

wasmFailValidateText('(module (func (result i32) (block (br 0))) (export "" 0))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (br 0)) (export "" 0))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (block (br_if 0 (i32.const 0)))) (export "" 0))', emptyStackError);

const DEPTH_OUT_OF_BOUNDS = /branch depth exceeds current nesting level/;

wasmFailValidateText('(module (func (br 1)))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (block (br 2))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (loop (br 2))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (if (i32.const 0) (br 2))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (if (i32.const 0) (br 1) (br 2))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (if (i32.const 0) (br 2) (br 1))))', DEPTH_OUT_OF_BOUNDS);

wasmFailValidateText('(module (func (br_if 1 (i32.const 0))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (block (br_if 2 (i32.const 0)))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (loop (br_if 2 (i32.const 0)))))', DEPTH_OUT_OF_BOUNDS);

wasmFailValidateText(`(module (func (result i32)
  block
    br 0
    if
      i32.const 0
      i32.const 2
    end
  end
) (export "" 0))`, unusedValuesError);

wasmFullPass(`(module (func (block $out (br_if $out (br 0)))) (export "run" 0))`, undefined);

wasmFullPass('(module (func (br 0)) (export "run" 0))', undefined);
wasmFullPass('(module (func (block (br 0))) (export "run" 0))', undefined);
wasmFullPass('(module (func (block $l (br $l))) (export "run" 0))', undefined);

wasmFullPass('(module (func (block (block (br 1)))) (export "run" 0))', undefined);
wasmFullPass('(module (func (block $l (block (br $l)))) (export "run" 0))', undefined);

wasmFullPass('(module (func (block $l (block $m (br $l)))) (export "run" 0))', undefined);
wasmFullPass('(module (func (block $l (block $m (br $m)))) (export "run" 0))', undefined);

wasmFullPass(`(module (func (result i32)
  (block
    (br 0)
    (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "run" 0))`, 1);

wasmFullPass(`(module (func (result i32)
  (block
    (block
      (br 0)
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "run" 0))`, 1);

wasmFullPass(`(module (func (result i32)
  (block $outer
    (block $inner
      (br $inner)
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "run" 0))`, 1);

var notcalled = false;
var called = false;
var imports = {"": {
    notcalled() {notcalled = true},
    called() {called = true}
}};
wasmFullPass(`(module
(import "" "notcalled")
(import "" "called")
(func
  (block
    (return (br 0))
    (call 0)
  )
  (call 1)
) (export "run" 2))`, undefined, imports);
assertEq(notcalled, false);
assertEq(called, true);

wasmFullPass(`(module (func
  (block
    (i32.add
      (i32.const 0)
      (return (br 0))
    )
    drop
  )
  (return)
) (export "run" 0))`, undefined);

wasmFullPass(`(module (func (result i32)
  (block
    (if
      (i32.const 1)
      (br 0)
      (return (i32.const 0))
    )
  )
  (return (i32.const 1))
) (export "run" 0))`, 1);

wasmFullPass('(module (func (br_if 0 (i32.const 1))) (export "run" 0))', undefined);
wasmFullPass('(module (func (br_if 0 (i32.const 0))) (export "run" 0))', undefined);
wasmFullPass('(module (func (block (br_if 0 (i32.const 1)))) (export "run" 0))', undefined);
wasmFullPass('(module (func (block (br_if 0 (i32.const 0)))) (export "run" 0))', undefined);
wasmFullPass('(module (func (block $l (br_if $l (i32.const 1)))) (export "run" 0))', undefined);

var isNonZero = wasmEvalText(`(module (func (result i32) (param i32)
  (block
    (br_if 0 (get_local 0))
    (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "" 0))`).exports[""];

assertEq(isNonZero(0), 0);
assertEq(isNonZero(1), 1);
assertEq(isNonZero(-1), 1);

// branches with values
// br/br_if and block
wasmFailValidateText('(module (func (result i32) (br 0)))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (br 0 (f32.const 42))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (result i32) (block (br 0))))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (block f32 (br 0 (f32.const 42)))))', mismatchError("f32", "i32"));

wasmFailValidateText(`(module (func (result i32) (param i32) (block (if i32 (get_local 0) (br 0 (i32.const 42))))) (export "" 0))`, /if without else with a result value/);
wasmFailValidateText(`(module (func (result i32) (param i32) (block i32 (if (get_local 0) (drop (i32.const 42))) (br 0 (f32.const 42)))) (export "" 0))`, mismatchError("f32", "i32"));

wasmFullPass('(module (func (result i32) (br 0 (i32.const 42)) (i32.const 13)) (export "run" 0))', 42);
wasmFullPass('(module (func (result i32) (block i32 (br 0 (i32.const 42)) (i32.const 13))) (export "run" 0))', 42);

wasmFailValidateText('(module (func) (func (block i32 (br 0 (call 0)) (i32.const 13))) (export "" 0))', emptyStackError);
wasmFailValidateText('(module (func) (func (block i32 (br_if 0 (call 0) (i32.const 1)) (i32.const 13))) (export "" 0))', emptyStackError);

var f = wasmEvalText(`(module (func (result i32) (param i32) (block i32 (if (get_local 0) (drop (i32.const 42))) (i32.const 43))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 43);

wasmFailValidateText(`(module (func (result i32) (param i32) (block i32 (if i32 (get_local 0) (br 0 (i32.const 42))) (i32.const 43))) (export "" 0))`, /if without else with a result value/);

var f = wasmEvalText(`(module (func (result i32) (param i32) (block i32 (if (get_local 0) (br 1 (i32.const 42))) (i32.const 43))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

wasmFailValidateText(`(module (func (result i32) (param i32) (block (br_if 0 (i32.const 42) (get_local 0)) (i32.const 43))) (export "" 0))`, /unused values not explicitly dropped by end of block/);

var f = wasmEvalText(`(module (func (result i32) (param i32) (block i32 (drop (br_if 0 (i32.const 42) (get_local 0))) (i32.const 43))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (result i32) (param i32) (block i32 (if (get_local 0) (drop (i32.const 42))) (br 0 (i32.const 43)))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 43);

wasmFailValidateText(`(module (func (result i32) (param i32) (block i32 (if i32 (get_local 0) (br 0 (i32.const 42))) (br 0 (i32.const 43)))) (export "" 0))`, /if without else with a result value/);

var f = wasmEvalText(`(module (func (result i32) (param i32) (if (get_local 0) (br 1 (i32.const 42))) (br 0 (i32.const 43))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (result i32) (param i32) (block i32 (if (get_local 0) (br 1 (i32.const 42))) (br 0 (i32.const 43)))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (result i32) (param i32) (br_if 0 (i32.const 42) (get_local 0)) (br 0 (i32.const 43))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (result i32) (param i32) (block i32 (br_if 0 (i32.const 42) (get_local 0)) (br 0 (i32.const 43)))) (export "" 0))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block i32 (if (get_local 0) (drop (i32.const 99))) (i32.const -1)))) (export "" 0))`).exports[""];
assertEq(f(0), 0);
assertEq(f(1), 0);

wasmFailValidateText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block i32 (if i32 (get_local 0) (br 0 (i32.const 99))) (i32.const -1)))) (export "" 0))`, /if without else with a result value/);

var f = wasmEvalText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block i32 (if (get_local 0) (br 1 (i32.const 99))) (i32.const -1)))) (export "" 0))`).exports[""];
assertEq(f(0), 0);
assertEq(f(1), 100);

wasmFailValidateText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block (br_if 0 (i32.const 99) (get_local 0)) (i32.const -1)))) (export "" 0))`, /unused values not explicitly dropped by end of block/);

var f = wasmEvalText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block i32 (drop (br_if 0 (i32.const 99) (get_local 0))) (i32.const -1)))) (export "" 0))`).exports[""];
assertEq(f(0), 0);
assertEq(f(1), 100);

wasmFullPass(`(module (func (result i32) (block i32 (br 0 (return (i32.const 42))) (i32.const 0))) (export "run" 0))`, 42);
wasmFullPass(`(module (func (result i32) (block i32 (return (br 0 (i32.const 42))))) (export "run" 0))`, 42);
wasmFullPass(`(module (func (result i32) (block i32 (return (br 0 (i32.const 42))) (i32.const 0))) (export "run" 0))`, 42);

wasmFullPass(`(module (func (result f32) (drop (block i32 (br 0 (i32.const 0)))) (block f32 (br 0 (f32.const 42)))) (export "run" 0))`, 42);

var called = 0;
var imports = {
    sideEffects: {
        ifTrue(x) {assertEq(x, 13); called++;},
        ifFalse(x) {assertEq(x, 37); called--;}
    }
}
var f = wasmEvalText(`(module
 (import "sideEffects" "ifTrue" (param i32))
 (import "sideEffects" "ifFalse" (param i32))
 (func
  (param i32) (result i32)
  (block $outer
   (if
    (get_local 0)
    (block (call 0 (i32.const 13)) (br $outer))
   )
   (if
    (i32.eqz (get_local 0))
    (block (call 1 (i32.const 37)) (br $outer))
   )
  )
  (i32.const 42)
 )
(export "" 2))`, imports).exports[""];
assertEq(f(0), 42);
assertEq(called, -1);
assertEq(f(1), 42);
assertEq(called, 0);

// br/br_if and loop
wasmFullPass(`(module (func (param i32) (result i32) (loop $out $in i32 (br $out (get_local 0)))) (export "run" 0))`, 1, {}, 1);
wasmFullPass(`(module (func (param i32) (result i32) (loop $in i32 (br 1 (get_local 0)))) (export "run" 0))`, 1, {}, 1);
wasmFullPass(`(module (func (param i32) (result i32) (block $out i32 (loop $in i32 (br $out (get_local 0))))) (export "run" 0))`, 1, {}, 1);

wasmFailValidateText(`(module (func (param i32) (result i32)
  (loop $out $in
   (if (get_local 0) (br $in (i32.const 1)))
   (if (get_local 0) (br $in (f32.const 2)))
   (if (get_local 0) (br $in (f64.const 3)))
   (if (get_local 0) (br $in))
   (i32.const 7)
  )
) (export "" 0))`, /unused values not explicitly dropped by end of block/);

wasmFullPass(`(module
 (func
  (result i32)
  (local i32)
  (block $out i32
    (loop $in i32
     (set_local 0 (i32.add (get_local 0) (i32.const 1)))
     (if
        (i32.ge_s (get_local 0) (i32.const 7))
        (br $out (get_local 0))
     )
     (br $in)
    )
  )
 )
(export "run" 0))`, 7);

wasmFullPass(`(module
 (func
  (result i32)
  (local i32)
  (block $out i32
   (loop $in i32
    (set_local 0 (i32.add (get_local 0) (i32.const 1)))
    (br_if $out (get_local 0) (i32.ge_s (get_local 0) (i32.const 7)))
    (br $in)
   )
  )
 )
(export "run" 0))`, 7);

// ----------------------------------------------------------------------------
// loop

wasmFailValidateText('(module (func (loop (br 2))))', DEPTH_OUT_OF_BOUNDS);

wasmFailValidateText('(module (func (result i32) (drop (loop (i32.const 2))) (i32.const 1)) (export "" 0))', /unused values not explicitly dropped by end of block/);
wasmFullPass('(module (func (loop)) (export "run" 0))', undefined);
wasmFullPass('(module (func (result i32) (loop (drop (i32.const 2))) (i32.const 1)) (export "run" 0))', 1);

wasmFullPass('(module (func (loop (br 1))) (export "run" 0))', undefined);
wasmFullPass('(module (func (loop $a (br 1))) (export "run" 0))', undefined);
wasmFullPass('(module (func (loop $a (br_if $a (i32.const 0)))) (export "run" 0))', undefined);
wasmFullPass('(module (func (loop $a $b (br $a))) (export "run" 0))', undefined);
wasmFullPass('(module (func (block $a (loop $b (br $a)))) (export "run" 0))', undefined);
wasmFullPass('(module (func (result i32) (loop i32 (i32.const 1))) (export "run" 0))', 1);

wasmFullPass(`(module (func (result i32) (local i32)
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
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (if
        (i32.gt_u (get_local 0) (i32.const 5))
        (br $break)
      )
      (set_local 0 (i32.add (get_local 0) (i32.const 1)))
      (br $continue)
    )
  )
  (return (get_local 0))
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
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
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (br_if
        $break
        (i32.gt_u (get_local 0) (i32.const 5))
      )
      (set_local 0 (i32.add (get_local 0) (i32.const 1)))
      (br $continue)
    )
  )
  (return (get_local 0))
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (loop
    $break $continue
    (set_local 0 (i32.add (get_local 0) (i32.const 1)))
    (br_if
      $continue
      (i32.le_u (get_local 0) (i32.const 5))
    )
  )
  (return (get_local 0))
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (set_local 0 (i32.add (get_local 0) (i32.const 1)))
      (br_if
        $continue
        (i32.le_u (get_local 0) (i32.const 5))
      )
    )
  )
  (return (get_local 0))
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
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
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
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
  )
  (return (get_local 0))
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
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
) (export "run" 0))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (set_local 0 (i32.add (get_local 0) (i32.const 1)))
      (loop
        (br_if
          $continue
          (i32.le_u (get_local 0) (i32.const 5))
        )
      )
      (br $break)
    )
  )
  (return (get_local 0))
) (export "run" 0))`, 6);

// ----------------------------------------------------------------------------
// br_table

wasmFailValidateText('(module (func (br_table 1 (i32.const 0))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (br_table 1 0 (i32.const 0))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (br_table 0 1 (i32.const 0))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (block (br_table 2 0 (i32.const 0)))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (block (br_table 0 2 (i32.const 0)))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (block (br_table 0 (f32.const 0)))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (loop (br_table 2 0 (i32.const 0)))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (loop (br_table 0 2 (i32.const 0)))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (loop (br_table 0 (f32.const 0)))))', mismatchError("f32", "i32"));

wasmFullPass(`(module (func (result i32) (param i32)
  (block $default
   (br_table $default (get_local 0))
   (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "run" 0))`, 1);

wasmFullPass(`(module (func (result i32) (param i32)
  (block $default
   (br_table $default (return (i32.const 1)))
   (return (i32.const 0))
  )
  (return (i32.const 2))
) (export "run" 0))`, 1);

wasmFullPass(`(module (func (result i32) (param i32)
  (block $outer
   (block $inner
    (br_table $inner (get_local 0))
    (return (i32.const 0))
   )
   (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "run" 0))`, 1);

var f = wasmEvalText(`(module (func (result i32) (param i32)
  (block $0
   (block $1
    (block $2
     (block $default
      (br_table $0 $1 $2 $default (get_local 0))
     )
     (return (i32.const -1))
    )
    (return (i32.const 2))
   )
  )
  (return (i32.const 0))
) (export "" 0))`).exports[""];

assertEq(f(-2), -1);
assertEq(f(-1), -1);
assertEq(f(0), 0);
assertEq(f(1), 0);
assertEq(f(2), 2);
assertEq(f(3), -1);

// br_table with values
wasmFailValidateText('(module (func (result i32) (block i32 (br_table 0 (i32.const 0)))))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (block i32 (br_table 0 (f32.const 0) (i32.const 0)))))', mismatchError("f32", "i32"));

wasmFailValidateText(`(module
 (func
  (result i32)
  (block $outer f32
   (block $inner f32
    (br_table $outer $inner (f32.const 13.37) (i32.const 1))
   )
   (br $outer (i32.const 42))
  )
 )
(export "" 0))`, mismatchError("i32", "f32"));

wasmFullPass(`(module (func (result i32) (block $default i32 (br_table $default (i32.const 42) (i32.const 1)))) (export "run" 0))`, 42);

var f = wasmEvalText(`(module (func (param i32) (result i32)
  (i32.add
   (block $1 i32
    (drop (block $0 i32
     (drop (block $default i32
      (br_table $0 $1 $default (get_local 0) (get_local 0))
     ))
     (tee_local 0 (i32.mul (i32.const 2) (get_local 0)))
    ))
    (tee_local 0 (i32.add (i32.const 4) (get_local 0)))
   )
   (i32.const 1)
  )
 ) (export "" 0))`).exports[""];

assertEq(f(0), 5);
assertEq(f(1), 2);
assertEq(f(2), 9);
assertEq(f(3), 11);
assertEq(f(4), 13);

// ----------------------------------------------------------------------------
// unreachable

const UNREACHABLE = /unreachable/;
assertErrorMessage(wasmEvalText(`(module (func (unreachable)) (export "" 0))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (if (unreachable) (nop))) (export "" 0))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (block (br_if 0 (unreachable)))) (export "" 0))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (block (br_table 0 (unreachable)))) (export "" 0))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (result i32) (i32.add (i32.const 0) (unreachable))) (export "" 0))`).exports[""], RuntimeError, UNREACHABLE);
