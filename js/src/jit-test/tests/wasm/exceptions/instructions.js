// Tests for Wasm exception proposal instructions.

// Test trivial try-catch with empty bodies.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (event $exn (type 0))
       (func (export "f") (result i32)
         try nop catch $exn end
         (i32.const 0)))`
  ).exports.f(),
  0
);

// Test try block with no throws
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (event $exn (type 0))
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
       (event $exn (type 0))
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
       (event $exn (type 0))
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
       (event $exn (type 0))
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
       (event $exn (type 0))
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
     (event $exn (type 0))
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
       (event $exn (type 0))
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
       (event $exn (type 0))
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
       (event $exn1 (type 0))
       (event $exn2 (type 0))
       (event $exn3 (type 0))
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
       (event $exn1 (type 0))
       (event $exn2 (type 0))
       (event $exn3 (type 0))
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
       (event $exn1 (type 0))
       (event $exn2 (type 0))
       (event $exn3 (type 0))
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
       (event $exn1 (type 0))
       (event $exn2 (type 0))
       (event $exn3 (type 0))
       (event $exn4 (type 0))
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
       (event $exn (type 0))
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

// Test br branching out of a catch block.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32)))
       (event $exn (type 0))
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
       (event $exn (type 0))
       (func (export "f") (result i32)
         i32.const 0
         return
         try nop catch $exn end))`
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
       (event $exn (type 0))
       (event $foo (type 1))
       (event $bar (type 2))
       (func (export "f") (result i32)
         try $l (result i32)
           (i32.const 42)
           (throw $exn)
         catch $exn
         end))`
  ).exports.f(),
  42
);

// Throw an exception carrying more than one value.
assertEq(
  wasmEvalText(
    `(module
       (type (func (param i32 i64 f32 f64)))
       (event $exn (type 0))
       (func (export "f") (result i32)
         try $l (result i32 i64 f32 f64)
           (i32.const 42)
           (i64.const 84)
           (f32.const 42.2)
           (f64.const 84.4)
           (throw $exn)
         catch $exn
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
       (event $exn (type 0))
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
       (event $exn1 (type 0))
       (event $exn2 (type 0))
       (func (export "f") (result i32)
         try (result i32)
           (i32.const 42)
           (throw $exn2)
         catch $exn1
         catch $exn2
         end))`
  ).exports.f(),
  42
);

// Try catch with block parameters.
assertEq(
  wasmEvalText(
    `(module
       (type (func))
       (event $exn (type 0))
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

// Ensure memory operations work after a throw. This is also testing that the
// WasmTlsReg/HeapReg are set correctly when throwing.
{
  let exports = wasmEvalText(
    `(module $m
       (memory $mem (data "bar"))
       (type (func))
       (event $exn (export "e") (type 0))
       (func (export "f")
         (throw $exn)))`
  ).exports;

  assertEq(
    wasmEvalText(
      `(module
         (type (func))
         (import "m" "e" (event $e (type 0)))
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
