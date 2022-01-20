// Tests for Wasm exception proposal instructions.

// Test try blocks with no handlers.
assertEq(
  wasmEvalText(
    `(module
       (func (export "f") (result i32)
         try (result i32) (i32.const 0) end))`
  ).exports.f(),
  0
);

assertEq(
  wasmEvalText(
    `(module
       (func (export "f") (result i32)
         try (result i32) (i32.const 0) (br 0) (i32.const 1) end))`
  ).exports.f(),
  0
);

assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try (result i32)
             (throw $exn)
             (i32.const 1)
           end
           drop
           (i32.const 2)
         catch $exn
           (i32.const 0)
         end))`
  ).exports.f(),
  0
);

assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try (result i32)
             try
               try
                 (throw $exn)
               end
             end
             (i32.const 1)
           end
           drop
           (i32.const 2)
         catch $exn
           (i32.const 0)
         end))`
  ).exports.f(),
  0
);

assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try (result i32)
             try
               try
                 (throw $exn)
               end
             catch_all
               rethrow 0
             end
             (i32.const 1)
           end
           drop
           (i32.const 2)
         catch $exn
           (i32.const 0)
         end))`
  ).exports.f(),
  0
);

// Test trivial try-catch with empty bodies.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try nop catch $exn end
         (i32.const 0)))`
  ).exports.f(),
  0
);

assertEq(
  wasmEvalText(
    `(module
       (func (export "f") (result i32)
         try nop catch_all end
         (i32.const 0)))`
  ).exports.f(),
  0
);

// Test try block with no throws
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           (i32.const 0)
         catch $exn
           (i32.const 1)
         end))`
  ).exports.f(),
  0
);

// Ensure catch block is really not run when no throw occurs.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32) (local i32)
         try
           (local.set 0 (i32.const 42))
         catch $exn
           (local.set 0 (i32.const 99))
         end
         (local.get 0)))`
  ).exports.f(),
  42
);

// Simple uses of throw.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           (i32.const 42)
           (throw $exn)
         catch $exn
           drop
           (i32.const 1)
         end))`
  ).exports.f(),
  1
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func $foo (param i32) (result i32)
         (local.get 0) (throw $exn))
       (func (export "f") (result i32)
         try (result i32)
           (i32.const 42)
           (call $foo)
         catch $exn
           drop
           (i32.const 1)
         end))`
  ).exports.f(),
  1
);

// Simple uses of throw of some Wasm vectortype values (Simd128).
if (wasmSimdEnabled()) {
  assertEq(
    wasmEvalText(
      `(module
         (type (func (param v128 v128 v128 v128)))
         (tag $exn (type 0))
         (func (export "f") (result i32)
           try (result i32)
             (v128.const i32x4 42 41 40 39)
             (v128.const f32x4 4.2 4.1 0.40 3.9)
             (v128.const i64x2 42 41)
             (v128.const f64x2 4.2 4.1)
             (throw $exn)
           catch $exn
             drop drop drop
             (i64x2.all_true)
           end))`
    ).exports.f(),
    1
  );

  assertEq(
    wasmEvalText(
      `(module
         (type (func (param v128 v128 v128 v128)))
         (tag $exn (type 0))
         (func $foo (param v128 v128 v128 v128) (result i32)
           (throw $exn (local.get 0)
                       (local.get 1)
                       (local.get 2)
                       (local.get 3)))
         (func (export "f") (result i32)
           try (result i32)
             (v128.const i32x4 42 41 40 39)
             (v128.const f32x4 4.2 4.1 0.40 3.9)
             (v128.const i64x2 42 41)
             (v128.const f64x2 4.2 4.1)
             (call $foo)
           catch $exn
             drop drop drop
             (i64x2.all_true)
           end))`
    ).exports.f(),
    1
  );

  {
    let imports =
        wasmEvalText(
          `(module
             (tag $exn (export "exn") (param v128))
             (func (export "throws") (param v128) (result v128)
               (throw $exn (local.get 0))
               (v128.const i32x4 9 10 11 12)))`).exports;

    let mod =
        `(module
           (import "m" "exn" (tag $exn (param v128)))
           (import "m" "throws" (func $throws (param v128) (result v128)))
           (func (export "f") (result i32) (local v128)
             (v128.const i32x4 1 2 3 4)
             (local.tee 0)
             (try (param v128) (result v128)
               (do (call $throws))
               (catch $exn)
               (catch_all (v128.const i32x4 5 6 7 8)))
             (local.get 0)
             (i32x4.eq)
             (i32x4.all_true)))`;

    assertEq(wasmEvalText(mod, { m : imports }).exports.f(), 1);
  }
}

// Further nested call frames should be ok.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func $foo (param i32) (result i32)
         (local.get 0) (call $bar))
       (func $bar (param i32) (result i32)
         (local.get 0) (call $quux))
       (func $quux (param i32) (result i32)
         (local.get 0) (throw $exn))
       (func (export "f") (result i32)
         try (result i32)
           (i32.const 42)
           (call $foo)
         catch $exn
           drop
           (i32.const 1)
         end))`
  ).exports.f(),
  1
);

// Basic throwing from loop.

assertEq(
  wasmEvalText(
    `(module
       (tag $exn)
       ;; For the purpose of this test, the params below should be increasing.
       (func (export "f") (param $depth_to_throw_exn i32)
                          (param $maximum_loop_iterations i32)
                          (result i32)
                          (local $loop_counter i32)
         ;; The loop is counting down.
         (local.get $maximum_loop_iterations)
         (local.set $loop_counter)
         (block $catch
           (loop $loop
             (if (i32.eqz (local.get $loop_counter))
                 (then
                   (return (i32.const 440)))
                 (else
                   (try
                     (do
                       (if (i32.eq (local.get $depth_to_throw_exn)
                                   (local.get $loop_counter))
                         (then
                           (throw $exn))
                         (else
                           (local.set $loop_counter
                                      (i32.sub (local.get $loop_counter)
                                               (i32.const 1))))))
                     (catch $exn (br $catch))
                     (catch_all))))
               (br $loop))
             (return (i32.const 10001)))
         (i32.const 10000)))`
  ).exports.f(2, 4),
  10000
);

// Ensure conditional throw works.
let conditional = wasmEvalText(
  `(module
     (type (func (param)))
     (tag $exn (type 0))
     (func (export "f") (param i32) (result i32)
       try (result i32)
         (local.get 0)
         if (result i32)
           (throw $exn)
         else
           (i32.const 42)
         end
       catch $exn
         (i32.const 99)
       end))`
).exports.f;

assertEq(conditional(0), 42);
assertEq(conditional(1), 99);

// Ensure multiple & nested try-catch blocks work.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func $foo (throw $exn))
       (func (export "f") (result i32) (local i32)
         try
           nop
         catch $exn
           (local.set 0 (i32.const 99))
         end
         try
           (call $foo)
         catch $exn
           (local.set 0 (i32.const 42))
         end
         (local.get 0)))`
  ).exports.f(),
  42
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func (export "f") (result i32) (local i32)
         try
           try
             try
               (throw $exn)
             catch $exn
               (local.set 0 (i32.const 42))
             end
           catch $exn
             (local.set 0 (i32.const 97))
           end
         catch $exn
           (local.set 0 (i32.const 98))
         end
         (local.get 0)))`
  ).exports.f(),
  42
);

assertEq(
  wasmEvalText(
    `(module
       (tag $exn)
       (func (export "f") (result i32)
         (try
           (do throw $exn)
           (catch $exn
             (try
               (do (throw $exn))
               (catch $exn))))
         (i32.const 27)))`
  ).exports.f(),
  27
);

{
  let nested_throw_in_block_and_in_catch =
      wasmEvalText(
        `(module
           (tag $exn)
           (func $throw
             (throw $exn))
           (func (export "f") (param $arg i32) (result i32)
             (block (result i32)
               (try (result i32)
                 (do
                   (call $throw)
                   (unreachable))
                 (catch $exn
                   (if (result i32)
                     (local.get $arg)
                     (then
                       (try (result i32)
                         (do
                           (call $throw)
                           (unreachable))
                         (catch $exn
                           (i32.const 27))))
                     (else
                       (i32.const 11))))))))`
      ).exports.f;

  assertEq(nested_throw_in_block_and_in_catch(1), 27);
  assertEq(nested_throw_in_block_and_in_catch(0), 11);
}

assertEq(
  wasmEvalText(
    `(module
       (tag $thrownExn)
       (tag $notThrownExn)
       (func (export "f") (result i32)
         (try (result i32)
           (do
             (try (result i32)
               (do (throw $thrownExn))
               (catch $notThrownExn
                 (i32.const 19))
               (catch $thrownExn
                 (i32.const 20))
               (catch_all
                 (i32.const 21)))))))`
  ).exports.f(),
  20
);

// Test that uncaught exceptions get propagated.
assertEq(
  wasmEvalText(
    `(module
       (tag $thrownExn)
       (tag $notThrownExn)
       (func (export "f") (result i32) (local i32)
         (try
           (do
             (try
               (do
                 (try
                   (do (throw $thrownExn))
                   (catch $notThrownExn
                     (local.set 0
                       (i32.or (local.get 0)
                               (i32.const 1))))))
               (catch $notThrownExn
                 (local.set 0
                   (i32.or (local.get 0)
                           (i32.const 2))))))
           (catch $thrownExn
             (local.set 0
               (i32.or (local.get 0)
                       (i32.const 4)))))
         (local.get 0)))`
  ).exports.f(),
  4
);

// Test tag dispatch for catches.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (tag $exn3 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           throw $exn1
         catch $exn1
           i32.const 1
         catch $exn2
           i32.const 2
         catch $exn3
           i32.const 3
         end))`
  ).exports.f(),
  1
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (tag $exn3 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           throw $exn2
         catch $exn1
           i32.const 1
         catch $exn2
           i32.const 2
         catch $exn3
           i32.const 3
         end))`
  ).exports.f(),
  2
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (tag $exn3 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           throw $exn3
         catch $exn1
           i32.const 1
         catch $exn2
           i32.const 2
         catch $exn3
           i32.const 3
         end))`
  ).exports.f(),
  3
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (tag $exn3 (type 0))
       (tag $exn4 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try (result i32)
             throw $exn4
           catch $exn1
             i32.const 1
           catch $exn2
             i32.const 2
           catch $exn3
             i32.const 3
           end
         catch $exn4
           i32.const 4
         end))`
  ).exports.f(),
  4
);

// Test usage of br before a throw.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try $l (result i32)
           (i32.const 2)
           (br $l)
           (throw $exn)
         catch $exn
           drop
           (i32.const 1)
         end))`
  ).exports.f(),
  2
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try $l (result i32)
           (throw $exn)
         catch $exn
           (i32.const 2)
           (br $l)
           rethrow 0
         end))`
  ).exports.f(),
  2
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try $l (result i32)
           (throw $exn)
         catch_all
           (i32.const 2)
           (br $l)
           rethrow 0
         end))`
  ).exports.f(),
  2
);

// Test br branching out of a catch block.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         block $l (result i32)
           block (result i32)
             try (result i32)
               (i32.const 42)
               (throw $exn)
             catch $exn
               br $l
               (i32.const 99)
             end
           end
         end))`
  ).exports.f(),
  42
);

// Test dead catch block.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         i32.const 0
         return
         try nop catch $exn end))`
  ).exports.f(),
  0
);

assertEq(
  wasmEvalText(
    `(module
       (func (export "f") (result i32)
         i32.const 0
         return
         try nop catch_all end))`
  ).exports.f(),
  0
);

// Test catch with exception values pushed to stack.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (type (func (param i32)))
       (type (func (param i64)))
       (tag $exn (type 0))
       (tag $foo (type 1))
       (tag $bar (type 2))
       (func (export "f") (result i32)
         try $l (result i32)
           (i32.const 42)
           (throw $exn)
         catch $exn
         catch_all
           (i32.const 99)
         end))`
  ).exports.f(),
  42
);

// Throw an exception carrying more than one value.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32 i64 f32 f64)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try $l (result i32 i64 f32 f64)
           (i32.const 42)
           (i64.const 84)
           (f32.const 42.2)
           (f64.const 84.4)
           (throw $exn)
         catch $exn
         catch_all
           (i32.const 99)
           (i64.const 999)
           (f32.const 99.9)
           (f64.const 999.9)
         end
         drop drop drop))`
  ).exports.f(),
  42
);

// This should also work inside nested frames.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32 i64 f32 f64)))
       (tag $exn (type 0))
       (func $foo (param i32 i64 f32 f64) (result i32 i64 f32 f64)
         (local.get 0)
         (local.get 1)
         (local.get 2)
         (local.get 3)
         (throw $exn))
       (func (export "f") (result i32)
         try $l (result i32 i64 f32 f64)
           (i32.const 42)
           (i64.const 84)
           (f32.const 42.2)
           (f64.const 84.4)
           (call $foo)
         catch $exn
         catch_all
           (i32.const 99)
           (i64.const 999)
           (f32.const 99.9)
           (f64.const 999.9)
         end
         drop drop drop))`
  ).exports.f(),
  42
);

// Multiple tagged catch in succession.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           (i32.const 42)
           (throw $exn2)
         catch $exn1
         catch $exn2
         catch_all
           (i32.const 99)
         end))`
  ).exports.f(),
  42
);

assertEq(
  wasmEvalText(
    `(module
       (tag $exn0)
       (tag $exn1)
       (tag $exn2)
       (tag $exn3)
       (tag $exn4)
       (tag $exn5)
       (func (export "f") (result i32)
         (try (result i32)
           (do (throw $exn4))
           (catch $exn5 (i32.const 5))
           (catch $exn2 (i32.const 2))
           (catch $exn4 (i32.const 4)) ;; Caught here.
           (catch $exn4 (i32.const 44)))))`
  ).exports.f(),
  4
);

// Try catch with block parameters.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         (i32.const 42)
         try (param i32) (result i32)
           nop
         catch $exn
           (i32.const 99)
         end))`
  ).exports.f(),
  42
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         (i32.const 42)
         try $l (param i32) (result i32)
           (throw $exn)
         catch $exn
         catch_all
           (i32.const 99)
         end))`
  ).exports.f(),
  42
);

// Test the catch_all case.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (func (export "f") (result i32)
         try $l (result i32)
           (i32.const 42)
           (throw $exn2)
         catch $exn1
         catch_all
           (i32.const 99)
         end))`
  ).exports.f(),
  99
);

assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param i32))
       (func (export "f") (result i32)
         try (result i32)
           try (result i32)
             (i32.const 42)
             (throw $exn)
           catch_all
             (i32.const 99)
           end
         catch $exn
         end))`
  ).exports.f(),
  99
);

// Test foreign exception catch.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (import "m" "foreign" (func $foreign))
       (tag $exn (type 0))
       (func (export "f") (result i32) (local i32)
         try $l
           (call $foreign)
         catch $exn
         catch_all
           (local.set 0 (i32.const 42))
         end
         (local.get 0)))`,
    {
      m: {
        foreign() {
          throw 5;
        },
      },
    }
  ).exports.f(),
  42
);

// Exception handlers should not catch traps.
assertErrorMessage(
  () =>
    wasmEvalText(
      `(module
         (type (func))
         (tag $exn (type 0))
         (func (export "f") (result i32) (local i32)
           try $l
             unreachable
           catch $exn
             (local.set 0 (i32.const 98))
           catch_all
             (local.set 0 (i32.const 99))
           end
           (local.get 0)))`
    ).exports.f(),
  WebAssembly.RuntimeError,
  "unreachable executed"
);

// Ensure that a RuntimeError created by the user is not filtered out
// as a trap emitted by the runtime (i.e., the filtering predicate is not
// observable from JS).
assertEq(
  wasmEvalText(
    `(module
       (import "m" "foreign" (func $foreign))
       (func (export "f") (result i32)
         try (result i32)
           (call $foreign)
           (i32.const 99)
         catch_all
           (i32.const 42)
         end))`,
    {
      m: {
        foreign() {
          throw new WebAssembly.RuntimeError();
        },
      },
    }
  ).exports.f(),
  42
);

// Test uncatchable JS exceptions (OOM & stack overflow).
{
  let f = wasmEvalText(
    `(module
       (import "m" "foreign" (func $foreign))
       (func (export "f") (result)
         try
           (call $foreign)
         catch_all
         end))`,
    {
      m: {
        foreign() {
          throwOutOfMemory();
        },
      },
    }
  ).exports.f;

  var thrownVal;
  try {
    f();
  } catch (exn) {
    thrownVal = exn;
  }

  assertEq(thrownVal, "out of memory");
}

assertErrorMessage(
  () =>
    wasmEvalText(
      `(module
       (import "m" "foreign" (func $foreign))
       (func (export "f")
         try
           (call $foreign)
         catch_all
         end))`,
      {
        m: {
          foreign: function foreign() {
            foreign();
          },
        },
      }
    ).exports.f(),
  Error,
  "too much recursion"
);

// Test all implemented instructions in a single module.
{
  let divFunctypeInline =
      `(param $numerator i32) (param $denominator i32) (result i32)`;

  let safediv = wasmEvalText(
    `(module
       (tag $divexn (param i32 i32))
       (tag $notThrownExn (param i32))
       (func $throwingdiv ${divFunctypeInline}
          (local.get $numerator)
          (local.get $denominator)
          (if (param i32 i32) (result i32)
            (i32.eqz (local.get $denominator))
            (then
              (try (param i32 i32)
                (do (throw $divexn))
                (delegate 0))
              (i32.const 9))
            (else
              i32.div_u)))
       (func $safediv (export "safediv") ${divFunctypeInline}
          (local.get $numerator)
          (local.get $denominator)
          (try (param i32 i32) (result i32)
            (do
              (call $throwingdiv))
            (catch $notThrownExn)
            (catch $divexn
              i32.add)
            (catch_all
              (i32.const 44)))))`
  ).exports.safediv;

  assertEq(safediv(6, 3), 2);
  assertEq(safediv(6, 0), 6);
}

// Test simple rethrow.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try
             throw $exn
           catch $exn
             rethrow 0
           end
           i32.const 1
         catch $exn
           i32.const 27
         end))`
  ).exports.f(),
  27
);

assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try
             throw $exn
           catch_all
             rethrow 0
           end
           i32.const 1
         catch $exn
           i32.const 27
         end))`
  ).exports.f(),
  27
);

// Test rethrows in nested blocks.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try
             throw $exn
           catch $exn
             block
               rethrow 1
             end
           end
           i32.const 1
         catch $exn
           i32.const 27
         end))`
  ).exports.f(),
  27
);

assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try
             throw $exn
           catch_all
             block
               rethrow 1
             end
           end
           i32.const 1
         catch $exn
           i32.const 27
         end))`
  ).exports.f(),
  27
);

assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try
             throw $exn1
           catch $exn1
             try
               throw $exn2
             catch $exn2
               rethrow 1
             end
           end
           i32.const 0
         catch $exn1
           i32.const 1
         catch $exn2
           i32.const 2
         end))`
  ).exports.f(),
  1
);

assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (tag $exn1 (type 0))
       (tag $exn2 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try
             throw $exn1
           catch $exn1
             try
               throw $exn2
             catch_all
               rethrow 1
             end
           end
           i32.const 0
         catch $exn1
           i32.const 1
         catch $exn2
           i32.const 2
         end))`
  ).exports.f(),
  1
);

// Test that rethrow makes the rest of the block dead code.
assertEq(
  wasmEvalText(
    `(module
       (tag (param i32))
       (func (export "f") (result i32)
         (try (result i32)
           (do (i32.const 1))
           (catch 0
             (rethrow 0)
             (i32.const 2)))))`
  ).exports.f(),
  1
);

assertEq(
  wasmEvalText(
    `(module
       (tag (param i32))
       (func (export "f") (result i32)
         (try (result i32)
           (do (try
                 (do (i32.const 13)
                     (throw 0))
                 (catch 0
                   (rethrow 0)))
               (unreachable))
           (catch 0))))`
  ).exports.f(),
  13
);

assertEq(
  wasmEvalText(
    `(module
       (tag)
       (func (export "f") (result i32)
         (try (result i32)
           (do
             (try
               (do (throw 0))
               (catch 0
                 (i32.const 4)
                 (rethrow 0)))
             (unreachable))
           (catch 0
              (i32.const 13)))))`
  ).exports.f(),
  13
);

assertEq(
  wasmEvalText(
    `(module
       (tag (param i32))
       (func (export "f") (result i32)
         (try (result i32)
           (do (try
                 (do (i32.const 13)
                     (throw 0))
                 (catch 0
                   (i32.const 111)
                   (rethrow 0)))
               (i32.const 222))
           (catch 0))))`
  ).exports.f(),
  13
);

// Test try-delegate blocks.

// Dead delegate to caller
assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         i32.const 1
         br 0
         try
           throw $exn
         delegate 0))`
  ).exports.f(),
  1
);

// Nested try-delegate.
assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         try (result i32)
           try
             try
               throw $exn
             delegate 0
           end
           i32.const 0
         catch $exn
           i32.const 1
         end))`
  ).exports.f(),
  1
);

// Non-throwing and breaking try-delegate.
assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         try (result i32)
           i32.const 1
           br 0
         delegate 0))`
    ).exports.f(),
    1
);

assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         try (result i32)
           i32.const 1
           return
         delegate 0))`
    ).exports.f(),
    1
  );

// More nested try-delegate.
assertEq(
    wasmEvalText(
      `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try
             i32.const 42
             throw $exn
           delegate 0
           i32.const 0
         catch $exn
           i32.const 1
           i32.add
         end))`
  ).exports.f(),
  43
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try (result i32)
             try
               i32.const 42
               throw $exn
             delegate 1
             i32.const 0
           catch $exn
             i32.const 1
             i32.add
           end
         catch $exn
           i32.const 2
           i32.add
         end))`
  ).exports.f(),
  44
);

assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           try (result i32)
             try (result i32)
               try
                 i32.const 42
                 throw $exn
               delegate 1
               i32.const 0
             catch $exn
               i32.const 1
               i32.add
             end
           delegate 0
         catch $exn
           i32.const 2
           i32.add
         end))`
  ).exports.f(),
  44
);

assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func $g (param i32) (result i32) (i32.const 42))
       (func (export "f") (result i32)
         try (result i32)
           try $t
             block (result i32)
               (i32.const 4)
               (call $g)
               try
                 throw $exn
               delegate $t
             end
             drop
           end
           i32.const 0
         catch_all
           i32.const 1
         end))`
  ).exports.f(),
  1
);

// Test delegation to function body and blocks.

// Non-throwing.
assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         try (result i32)
           i32.const 1
         delegate 0))`
  ).exports.f(),
  1
);

// Block target.
assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param i32))
       (func (export "f") (result i32)
         try (result i32)
           block
             try
               i32.const 1
               throw $exn
             delegate 0
           end
           i32.const 0
         catch $exn
         end))`
  ).exports.f(),
  1
);

// Catch target.
assertEq(
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         try (result i32)
           try
             throw $exn
           catch $exn
             try
               throw $exn
             delegate 0
           end
           i32.const 0
         catch_all
           i32.const 1
         end))`
  ).exports.f(),
  1
);

// Target function body.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (func (export "f") (result i32)
         try (result i32)
           call $g
         catch $exn
         end)
       (func $g (result i32)
         try (result i32)
           try
             i32.const 42
             throw $exn
           delegate 1
           i32.const 0
         catch $exn
           i32.const 1
           i32.add
         end))`
  ).exports.f(),
  42
);

// Try-delegate from inside a loop.
assertEq(
  wasmEvalText(
    `(module
       (tag $exn)
       ;; For the purpose of this test, the params below should be increasing.
       (func (export "f") (param $depth_to_throw_exn i32)
                          (param $maximum_loop_iterations i32)
                          (result i32)
                          (local $loop_countdown i32)
                          ;; Counts how many times the loop was started.
                          (local $loop_verifier i32)
         ;; The loop is counting down.
         (local.get $maximum_loop_iterations)
         (local.set $loop_countdown)
         (try $catch_exn (result i32)
           (do
             (try
               (do
                 (loop $loop
                   ;; Counts how many times the loop was started.
                   (local.set $loop_verifier
                              (i32.add (i32.const 1)
                                       (local.get $loop_verifier)))
                   (if (i32.eqz (local.get $loop_countdown))
                     (then (return (i32.const 440)))
                     (else
                       (try $rethrow_label
                         (do
                           (if (i32.eq (local.get $depth_to_throw_exn)
                                       (local.get $loop_countdown))
                               (then (throw $exn))
                               (else
                                 (local.set $loop_countdown
                                            (i32.sub (local.get $loop_countdown)
                                                     (i32.const 1))))))
                         (catch $exn (try
                                       (do (rethrow $rethrow_label))
                                       (delegate $catch_exn))))))
                   (br $loop)))
               (catch_all unreachable))
             (i32.const 2000))
           (catch_all (i32.const 10000)))
         (i32.add (local.get $loop_verifier))))`
  ).exports.f(3, 5),
  10003
);
