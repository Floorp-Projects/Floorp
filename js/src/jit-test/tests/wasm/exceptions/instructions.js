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

// Ensure memory operations work after a throw. This is also testing that the
// WasmTlsReg/HeapReg are set correctly when throwing.
{
  let exports = wasmEvalText(
    `(module $m
       (memory $mem (data "bar"))
       (type (func))
       (tag $exn (export "e") (type 0))
       (func (export "f")
         (throw $exn)))`
  ).exports;

  assertEq(
    wasmEvalText(
      `(module
         (type (func))
         (import "m" "e" (tag $e (type 0)))
         (import "m" "f" (func $foreign))
         (memory $mem (data "foo"))
         (func (export "f") (result i32)
           try
             call $foreign
           catch $e
           end
           (i32.const 0)
           (i32.load8_u)))`,
      { m: exports }
    ).exports.f(),
    102
  );
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

// Test try-delegate blocks.
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

// Test delegation to function body.
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
