const RuntimeError = WebAssembly.RuntimeError;

// ----------------------------------------------------------------------------
// if

// Condition is an int32
wasmFailValidateText('(module (func (local f32) (if (local.get 0) (then (i32.const 1)))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (local f32) (if (local.get 0) (then (i32.const 1)) (else (i32.const 0)))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (local f64) (if (local.get 0) (then (i32.const 1)) (else (i32.const 0)))))', mismatchError("f64", "i32"));
wasmEvalText('(module (func (local i32) (if (local.get 0) (then (nop)))) (export "" (func 0)))');
wasmEvalText('(module (func (local i32) (if (local.get 0) (then (nop)) (else (nop)))) (export "" (func 0)))');

// Expression values types are consistent
// Also test that we support (result t) for `if`
wasmFailValidateText('(module (func (result i32) (local f32) (if (result f32) (i32.const 42) (then (local.get 0)) (else (i32.const 0)))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (result i32) (local f64) (if (result i32) (i32.const 42) (then (i32.const 0)) (else (local.get 0)))))', mismatchError("f64", "i32"));
wasmFailValidateText('(module (func (result i64) (if (result i64) (i32.const 0) (then (i32.const 1)) (else (i32.const 2)))))', mismatchError("i32", "i64"));
assertEq(wasmEvalText('(module (func (result i32) (if (result i32) (i32.const 42) (then (i32.const 1)) (else (i32.const 2)))) (export "" (func 0)))').exports[""](), 1);
assertEq(wasmEvalText('(module (func (result i32) (if (result i32) (i32.const 0) (then (i32.const 1)) (else (i32.const 2)))) (export "" (func 0)))').exports[""](), 2);

// Even if we don't yield, sub expressions types still have to match.
wasmFailValidateText('(module (func (param f32) (if (result i32) (i32.const 42) (then (i32.const 1)) (else (local.get 0)))) (export "" (func 0)))', mismatchError('f32', 'i32'));
wasmFailValidateText('(module (func (if (result i32) (i32.const 42) (then (i32.const 1)) (else (i32.const 0)))) (export "" (func 0)))', /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/);
wasmFullPass('(module (func (drop (if (result i32) (i32.const 42) (then (i32.const 1)) (else (i32.const 0))))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (param f32) (if (i32.const 42) (then (drop (i32.const 1))) (else (drop (local.get 0))))) (export "run" (func 0)))', undefined, {}, 13.37);

// Sub-expression values are returned
// Also test that we support (result t) for `block`
wasmFullPass(`(module
    (func
        (result i32)
        (if (result i32)
            (i32.const 42)
            (then
              (block (result i32)
                (if (result i32)
                    (block (result i32)
                        (drop (i32.const 3))
                        (drop (i32.const 5))
                        (i32.const 0)
                    )
                    (then (i32.const 1))
                    (else (i32.const 2))
                )
              )
            )
            (else (i32.const 0))
        )
    )
    (export "run" (func 0))
)`, 2);

// The if (resp. else) branch is taken iff the condition is true (resp. false)
counter = 0;
var imports = { "":{inc() { counter++ }} };
wasmFullPass(`(module
    (import "" "inc" (func (result i32)))
    (func
        (result i32)
        (if (result i32)
            (i32.const 42)
            (then (i32.const 1))
            (else (call 0))
        )
    )
    (export "run" (func 1))
)`, 1, imports);
assertEq(counter, 0);

wasmFullPass(`(module
    (import "" "inc" (func (result i32)))
    (func
        (result i32)
        (if (result i32)
            (i32.const 0)
            (then (call 0))
            (else (i32.const 1))
        )
    )
    (export "run" (func 1))
)`, 1, imports);
assertEq(counter, 0);

wasmFullPass(`(module
    (import "" "inc" (func (result i32)))
    (func
        (if
            (i32.const 0)
            (then (drop (call 0)))
        )
    )
    (export "run" (func 1))
)`, undefined, imports);
assertEq(counter, 0);

assertEq(wasmEvalText(`(module
    (import "" "inc" (func (result i32)))
    (func
        (if
            (i32.const 1)
            (then (drop (call 0)))
        )
    )
    (export "" (func 1))
)`, imports).exports[""](), undefined);
assertEq(counter, 1);

wasmFullPass(`(module
    (func
        (result i32)
        (if (result i32)
            (i32.const 0)
            (then (br 0 (i32.const 0)))
            (else (br 0 (i32.const 1)))
        )
    )
    (export "run" (func 0))
)`, 1);
assertEq(counter, 1);

wasmFullPass(`(module
    (func
        (if
            (i32.const 1)
            (then (br 0))
        )
    )
    (export "run" (func 0))
)`, undefined);
assertEq(counter, 1);

// One can chain if with if/if
counter = 0;
wasmFullPass(`(module
  (import "" "inc" (func (result i32)))
  (func
      (result i32)
      (if (result i32)
          (i32.const 1)
          (then
              (if (result i32)
                  (i32.const 2)
                  (then
                      (if (result i32)
                          (i32.const 3)
                          (then
                              (if (result i32)
                                  (i32.const 0)
                                  (then (call 0))
                                  (else (i32.const 42))
                              )
                          )
                          (else (call 0))
                      )
                  )
                  (else (call 0))
              )
          )
          (else (call 0))
      )
  )
  (export "run" (func 1))
)`, 42, imports);
assertEq(counter, 0);

// "if" doesn't return an expression value
wasmFailValidateText('(module (func (result i32) (if (result i32) (i32.const 42) (then (i32.const 0)))))', /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);
wasmFailValidateText('(module (func (result i32) (if (result i32) (i32.const 42) (then (drop (i32.const 0))))))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (if (result i32) (i32.const 1) (then (i32.const 0)) (else (if (result i32) (i32.const 1) (then (i32.const 1)))))))', /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);
wasmFailValidateText('(module (func (result i32) (if (result i32) (i32.const 1) (then (drop (i32.const 0))) (else (if (i32.const 1) (then (drop (i32.const 1))))))))', emptyStackError);
wasmFailValidateText('(module (func (if (result i32) (i32.const 1) (then (i32.const 0)) (else (if (result i32) (i32.const 1) (then (i32.const 1)))))))', /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);
wasmFailValidateText('(module (func (if (result i32) (i32.const 1) (then (i32.const 0)) (else (if (i32.const 1) (then (drop (i32.const 1))))))))', emptyStackError);
wasmFailValidateText('(module (func (if (i32.const 1) (then (drop (i32.const 0))) (else (if (result i32) (i32.const 1) (then (i32.const 1)))))))', /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);
wasmEvalText('(module (func (if (i32.const 1) (then (drop (i32.const 0))) (else (if (i32.const 1) (then (drop (i32.const 1))))))))');

// ----------------------------------------------------------------------------
// return

wasmFullPass('(module (func (return)) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (result i32) (return (i32.const 1))) (export "run" (func 0)))', 1);
wasmFailValidateText('(module (func (if (return) (then (i32.const 0)))) (export "run" (func 0)))', unusedValuesError);
wasmFailValidateText('(module (func (result i32) (return)) (export "" (func 0)))', emptyStackError);
wasmFullPass('(module (func (return (i32.const 1))) (export "run" (func 0)))', undefined);
wasmFailValidateText('(module (func (result f32) (return (i32.const 1))) (export "" (func 0)))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (result i32) (return)) (export "" (func 0)))', emptyStackError);

// ----------------------------------------------------------------------------
// br / br_if

wasmFailValidateText('(module (func (result i32) (block (br 0))) (export "" (func 0)))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (br 0)) (export "" (func 0)))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (block (br_if 0 (i32.const 0)))) (export "" (func 0)))', emptyStackError);

const DEPTH_OUT_OF_BOUNDS = /(branch depth exceeds current nesting level)|(branch depth too large)/;

wasmFailValidateText('(module (func (br 1)))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (block (br 2))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (loop (br 2))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (if (i32.const 0) (then (br 2)))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (if (i32.const 0) (then (br 1)) (else (br 2)))))', DEPTH_OUT_OF_BOUNDS);
wasmFailValidateText('(module (func (if (i32.const 0) (then (br 2)) (else (br 1)))))', DEPTH_OUT_OF_BOUNDS);

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
) (export "" (func 0)))`, unusedValuesError);

wasmFullPass(`(module (func (block $out (br_if $out (br 0)))) (export "run" (func 0)))`, undefined);

wasmFullPass('(module (func (br 0)) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block (br 0))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block $l (br $l))) (export "run" (func 0)))', undefined);

wasmFullPass('(module (func (block (block (br 1)))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block $l (block (br $l)))) (export "run" (func 0)))', undefined);

wasmFullPass('(module (func (block $l (block $m (br $l)))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block $l (block $m (br $m)))) (export "run" (func 0)))', undefined);

wasmFullPass(`(module (func (result i32)
  (block
    (br 0)
    (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "run" (func 0)))`, 1);

wasmFullPass(`(module (func (result i32)
  (block
    (block
      (br 0)
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "run" (func 0)))`, 1);

wasmFullPass(`(module (func (result i32)
  (block $outer
    (block $inner
      (br $inner)
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "run" (func 0)))`, 1);

var notcalled = false;
var called = false;
var imports = {"": {
    notcalled() {notcalled = true},
    called() {called = true}
}};
wasmFullPass(`(module
(import "" "notcalled" (func))
(import "" "called" (func))
(func
  (block
    (return (br 0))
    (call 0)
  )
  (call 1)
) (export "run" (func 2)))`, undefined, imports);
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
) (export "run" (func 0)))`, undefined);

wasmFullPass(`(module (func (result i32)
  (block
    (if
      (i32.const 1)
      (then (br 0))
      (else (return (i32.const 0)))
    )
  )
  (return (i32.const 1))
) (export "run" (func 0)))`, 1);

wasmFullPass('(module (func (br_if 0 (i32.const 1))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (br_if 0 (i32.const 0))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block (br_if 0 (i32.const 1)))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block (br_if 0 (i32.const 0)))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block $l (br_if $l (i32.const 1)))) (export "run" (func 0)))', undefined);

var isNonZero = wasmEvalText(`(module (func (param i32) (result i32)
  (block
    (br_if 0 (local.get 0))
    (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "" (func 0)))`).exports[""];

assertEq(isNonZero(0), 0);
assertEq(isNonZero(1), 1);
assertEq(isNonZero(-1), 1);

// branches with values
// br/br_if and block
wasmFailValidateText('(module (func (result i32) (br 0)))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (br 0 (f32.const 42))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (result i32) (block (br 0))))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (block (result f32) (br 0 (f32.const 42)))))', mismatchError("f32", "i32"));

wasmFailValidateText(`(module (func (param i32) (result i32) (block (if (result i32) (local.get 0) (then (br 0 (i32.const 42)))))) (export "" (func 0)))`, /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);
wasmFailValidateText(`(module (func (param i32) (result i32) (block (result i32) (if (local.get 0) (then (drop (i32.const 42)))) (else (br 0 (f32.const 42))))) (export "" (func 0)))`, mismatchError("f32", "i32"));

wasmFullPass('(module (func (result i32) (br 0 (i32.const 42)) (i32.const 13)) (export "run" (func 0)))', 42);
wasmFullPass('(module (func (result i32) (block (result i32) (br 0 (i32.const 42)) (i32.const 13))) (export "run" (func 0)))', 42);

wasmFailValidateText('(module (func) (func (block (result i32) (br 0 (call 0)) (i32.const 13))) (export "" (func 0)))', emptyStackError);
wasmFailValidateText('(module (func) (func (block (result i32) (br_if 0 (call 0) (i32.const 1)) (i32.const 13))) (export "" (func 0)))', emptyStackError);

var f = wasmEvalText(`(module (func (param i32) (result i32) (block (result i32) (if (local.get 0) (then (drop (i32.const 42)))) (i32.const 43))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 43);

wasmFailValidateText(`(module (func (param i32) (result i32) (block (result i32) (if (result i32) (local.get 0) (then (br 0 (i32.const 42)))) (i32.const 43))) (export "" (func 0)))`, /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);

var f = wasmEvalText(`(module (func (param i32) (result i32) (block (result i32) (if (local.get 0) (then (br 1 (i32.const 42)))) (i32.const 43))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

wasmFailValidateText(`(module (func (param i32) (result i32) (block (br_if 0 (i32.const 42) (local.get 0)) (i32.const 43))) (export "" (func 0)))`, /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/);

var f = wasmEvalText(`(module (func (param i32) (result i32) (block (result i32) (drop (br_if 0 (i32.const 42) (local.get 0))) (i32.const 43))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (param i32) (result i32) (block (result i32) (if (local.get 0) (then (drop (i32.const 42)))) (br 0 (i32.const 43)))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 43);

wasmFailValidateText(`(module (func (param i32) (result i32) (block (result i32) (if (result i32) (local.get 0) (then (br 0 (i32.const 42)))) (br 0 (i32.const 43)))) (export "" (func 0)))`, /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);

var f = wasmEvalText(`(module (func (param i32) (result i32) (if (local.get 0) (then (br 1 (i32.const 42)))) (br 0 (i32.const 43))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (param i32) (result i32) (block (result i32) (if (local.get 0) (then (br 1 (i32.const 42)))) (br 0 (i32.const 43)))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (param i32) (result i32) (br_if 0 (i32.const 42) (local.get 0)) (br 0 (i32.const 43))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (param i32) (result i32) (block (result i32) (br_if 0 (i32.const 42) (local.get 0)) (br 0 (i32.const 43)))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 43);
assertEq(f(1), 42);

var f = wasmEvalText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block (result i32) (if (local.get 0) (then (drop (i32.const 99)))) (i32.const -1)))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 0);
assertEq(f(1), 0);

wasmFailValidateText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block (result i32) (if (result i32) (local.get 0) (then (br 0 (i32.const 99)))) (i32.const -1)))) (export "" (func 0)))`, /(if without else with a result value)|(type mismatch: expected i32 but nothing on stack)/);

var f = wasmEvalText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block (result i32) (if (local.get 0) (then (br 1 (i32.const 99)))) (i32.const -1)))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 0);
assertEq(f(1), 100);

wasmFailValidateText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block (br_if 0 (i32.const 99) (local.get 0)) (i32.const -1)))) (export "" (func 0)))`, /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/);

var f = wasmEvalText(`(module (func (param i32) (result i32) (i32.add (i32.const 1) (block (result i32) (drop (br_if 0 (i32.const 99) (local.get 0))) (i32.const -1)))) (export "" (func 0)))`).exports[""];
assertEq(f(0), 0);
assertEq(f(1), 100);

wasmFullPass(`(module (func (result i32) (block (result i32) (br 0 (return (i32.const 42))) (i32.const 0))) (export "run" (func 0)))`, 42);
wasmFullPass(`(module (func (result i32) (block (result i32) (return (br 0 (i32.const 42))))) (export "run" (func 0)))`, 42);
wasmFullPass(`(module (func (result i32) (block (result i32) (return (br 0 (i32.const 42))) (i32.const 0))) (export "run" (func 0)))`, 42);

wasmFullPass(`(module (func (result f32) (drop (block (result i32) (br 0 (i32.const 0)))) (block (result f32) (br 0 (f32.const 42)))) (export "run" (func 0)))`, 42);

var called = 0;
var imports = {
    sideEffects: {
        ifTrue(x) {assertEq(x, 13); called++;},
        ifFalse(x) {assertEq(x, 37); called--;}
    }
}
var f = wasmEvalText(`(module
 (import "sideEffects" "ifTrue" (func (param i32)))
 (import "sideEffects" "ifFalse" (func (param i32)))
 (func
  (param i32) (result i32)
  (block $outer
   (if
    (local.get 0)
    (then (block (call 0 (i32.const 13)) (br $outer)))
   )
   (if
    (i32.eqz (local.get 0))
    (then (block (call 1 (i32.const 37)) (br $outer)))
   )
  )
  (i32.const 42)
 )
(export "" (func 2)))`, imports).exports[""];
assertEq(f(0), 42);
assertEq(called, -1);
assertEq(f(1), 42);
assertEq(called, 0);

// br/br_if and loop
wasmFullPass(`(module (func (param i32) (result i32) (block $out (result i32) (loop $in (result i32) (br $out (local.get 0))))) (export "run" (func 0)))`, 1, {}, 1);
wasmFullPass(`(module (func (param i32) (result i32) (loop $in (result i32) (br 1 (local.get 0)))) (export "run" (func 0)))`, 1, {}, 1);
wasmFullPass(`(module (func (param i32) (result i32) (block $out (result i32) (loop $in (result i32) (br $out (local.get 0))))) (export "run" (func 0)))`, 1, {}, 1);

wasmFailValidateText(`(module (func (param i32) (result i32)
  (block $out
   (loop $in
    (if (local.get 0) (then (br $in (i32.const 1))))
    (if (local.get 0) (then (br $in (f32.const 2))))
    (if (local.get 0) (then (br $in (f64.const 3))))
    (if (local.get 0) (then (br $in)))
    (i32.const 7)
   )
  )
) (export "" (func 0)))`, /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/);

wasmFullPass(`(module
 (func
  (result i32)
  (local i32)
  (block $out (result i32)
    (loop $in (result i32)
     (local.set 0 (i32.add (local.get 0) (i32.const 1)))
     (if
        (i32.ge_s (local.get 0) (i32.const 7))
        (then (br $out (local.get 0)))
     )
     (br $in)
    )
  )
 )
(export "run" (func 0)))`, 7);

wasmFullPass(`(module
 (func
  (result i32)
  (local i32)
  (block $out (result i32)
   (loop $in (result i32)
    (local.set 0 (i32.add (local.get 0) (i32.const 1)))
    (br_if $out (local.get 0) (i32.ge_s (local.get 0) (i32.const 7)))
    (br $in)
   )
  )
 )
(export "run" (func 0)))`, 7);

// ----------------------------------------------------------------------------
// loop

wasmFailValidateText('(module (func (loop (br 2))))', DEPTH_OUT_OF_BOUNDS);

wasmFailValidateText('(module (func (result i32) (drop (loop (i32.const 2))) (i32.const 1)) (export "" (func 0)))', /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/);
wasmFullPass('(module (func (loop)) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (result i32) (loop (drop (i32.const 2))) (i32.const 1)) (export "run" (func 0)))', 1);

wasmFullPass('(module (func (loop (br 1))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (loop $a (br 1))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (loop $a (br_if $a (i32.const 0)))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (block $a (loop $b (br $a)))) (export "run" (func 0)))', undefined);
wasmFullPass('(module (func (result i32) (loop (result i32) (i32.const 1))) (export "run" (func 0)))', 1);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (if
        (i32.gt_u (local.get 0) (i32.const 5))
        (then (br $break))
      )
      (local.set 0 (i32.add (local.get 0) (i32.const 1)))
      (br $continue)
    )
  )
  (return (local.get 0))
) (export "run" (func 0)))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (br_if
        $break
        (i32.gt_u (local.get 0) (i32.const 5))
      )
      (local.set 0 (i32.add (local.get 0) (i32.const 1)))
      (br $continue)
    )
  )
  (return (local.get 0))
) (export "run" (func 0)))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (local.set 0 (i32.add (local.get 0) (i32.const 1)))
      (br_if
        $continue
        (i32.le_u (local.get 0) (i32.const 5))
      )
    )
  )
  (return (local.get 0))
) (export "run" (func 0)))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (br_if
        $break
        (i32.gt_u (local.get 0) (i32.const 5))
      )
      (local.set 0 (i32.add (local.get 0) (i32.const 1)))
      (loop
        (br $continue)
      )
      (return (i32.const 42))
    )
  )
  (return (local.get 0))
) (export "run" (func 0)))`, 6);

wasmFullPass(`(module (func (result i32) (local i32)
  (block
    $break
    (loop
      $continue
      (local.set 0 (i32.add (local.get 0) (i32.const 1)))
      (loop
        (br_if
          $continue
          (i32.le_u (local.get 0) (i32.const 5))
        )
      )
      (br $break)
    )
  )
  (return (local.get 0))
) (export "run" (func 0)))`, 6);

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

wasmFullPass(`(module (func (param i32) (result i32)
  (block $default
   (br_table $default (local.get 0))
   (return (i32.const 0))
  )
  (return (i32.const 1))
) (export "run" (func 0)))`, 1);

wasmFullPass(`(module (func (param i32) (result i32)
  (block $default
   (br_table $default (return (i32.const 1)))
   (return (i32.const 0))
  )
  (return (i32.const 2))
) (export "run" (func 0)))`, 1);

wasmFullPass(`(module (func (param i32) (result i32)
  (block $outer
   (block $inner
    (br_table $inner (local.get 0))
    (return (i32.const 0))
   )
   (return (i32.const 1))
  )
  (return (i32.const 2))
) (export "run" (func 0)))`, 1);

var f = wasmEvalText(`(module (func (param i32) (result i32)
  (block $0
   (block $1
    (block $2
     (block $default
      (br_table $0 $1 $2 $default (local.get 0))
     )
     (return (i32.const -1))
    )
    (return (i32.const 2))
   )
  )
  (return (i32.const 0))
) (export "" (func 0)))`).exports[""];

assertEq(f(-2), -1);
assertEq(f(-1), -1);
assertEq(f(0), 0);
assertEq(f(1), 0);
assertEq(f(2), 2);
assertEq(f(3), -1);

// br_table with values
wasmFailValidateText('(module (func (result i32) (block (result i32) (br_table 0 (i32.const 0)))))', emptyStackError);
wasmFailValidateText('(module (func (result i32) (block (result i32) (br_table 0 (f32.const 0) (i32.const 0)))))', mismatchError("f32", "i32"));

wasmFailValidateText(`(module
 (func
  (result i32)
  (block $outer (result f32)
   (block $inner (result f32)
    (br_table $outer $inner (f32.const 13.37) (i32.const 1))
   )
   (br $outer (i32.const 42))
  )
 )
(export "" (func 0)))`, mismatchError("i32", "f32"));

wasmFullPass(`(module (func (result i32) (block $default (result i32) (br_table $default (i32.const 42) (i32.const 1)))) (export "run" (func 0)))`, 42);

var f = wasmEvalText(`(module (func (param i32) (result i32)
  (i32.add
   (block $1 (result i32)
    (drop (block $0 (result i32)
     (drop (block $default (result i32)
      (br_table $0 $1 $default (local.get 0) (local.get 0))
     ))
     (local.tee 0 (i32.mul (i32.const 2) (local.get 0)))
    ))
    (local.tee 0 (i32.add (i32.const 4) (local.get 0)))
   )
   (i32.const 1)
  )
 ) (export "" (func 0)))`).exports[""];

assertEq(f(0), 5);
assertEq(f(1), 2);
assertEq(f(2), 9);
assertEq(f(3), 11);
assertEq(f(4), 13);

// ----------------------------------------------------------------------------
// unreachable

const UNREACHABLE = /unreachable/;
assertErrorMessage(wasmEvalText(`(module (func (unreachable)) (export "" (func 0)))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (if (unreachable) (then (nop)))) (export "" (func 0)))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (block (br_if 0 (unreachable)))) (export "" (func 0)))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (block (br_table 0 (unreachable)))) (export "" (func 0)))`).exports[""], RuntimeError, UNREACHABLE);
assertErrorMessage(wasmEvalText(`(module (func (result i32) (i32.add (i32.const 0) (unreachable))) (export "" (func 0)))`).exports[""], RuntimeError, UNREACHABLE);
