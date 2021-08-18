// Tests for throwing exceptions to JS from Wasm.

function assertWasmThrowsExn(thunk) {
  let thrown = false;

  try {
    thunk();
  } catch (exn) {
    thrown = true;
    assertEq(exn instanceof WebAssembly.Exception, true);
  }

  assertEq(thrown, true, "missing exception");
}

// Test that handler-less trys don't catch anything.
assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func (export "f")
         try (throw $exn) end))`
  ).exports.f()
);

assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func $g (throw $exn))
       (func (export "f")
         try (call $g) end)
)`
  ).exports.f()
);

assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func (export "f")
         try try (throw $exn) end end))`
  ).exports.f()
);

assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f")
         try
           try
             throw $exn
           delegate 0
         end))`
  ).exports.f()
);

assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f")
         try
           try
             throw $exn
           delegate 1
         end))`
  ).exports.f()
);

// Test throwing simple empty exceptions to JS.
assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (type (func (param)))
       (tag $exn (type 0))
       (func (export "f")
         (throw $exn)))`
  ).exports.f()
);

// Test that wasm preserves the values of non-object exceptions that pass
// through it back to JS.
assertThrowsValue(
  () =>
    wasmEvalText(
      `(module
         (tag $exn)
         (import "m" "import" (func $import))
         (func (export "f")
           try
             (call $import)
           catch $exn
             ;; this block shouldn't be reached
           end))`,
      {
        m: {
          import: () => {
            throw 42;
          },
        },
      }
    ).exports.f(),
  42
);

// Like previous test, but using a rethrow instruction instead.
assertThrowsValue(
  () =>
    wasmEvalText(
      `(module
         (import "m" "import" (func $import))
         (func (export "f")
           try
             (call $import)
           catch_all
             (rethrow 0)
           end))`,
      {
        m: {
          import: () => {
            throw 42;
          },
        },
      }
    ).exports.f(),
  42
);

// Test for throwing to JS and then back to Wasm.
{
  var wasmThrower;
  let exports = wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (type 0))
       (import "m" "import" (func $import (result i32)))
       (func (export "thrower")
         (i32.const 42)
         (throw $exn))
       (func (export "catcher") (result i32)
         try (result i32)
           (call $import)
         catch $exn
         end))`,
    {
      m: {
        import: () => {
          return wasmThrower();
        },
      },
    }
  ).exports;

  wasmThrower = exports.thrower;
  assertEq(exports.catcher(), 42);
}

// Tests for checking the tags of exceptions.
{
  let exports = wasmEvalText(
    `(module
       (type (func (param i32)))
       (tag $exn (export "exn") (type 0))
       (func (export "thrower")
         (i32.const 42)
         (throw $exn)))`
  ).exports;

  let imports = {
    store: {
      throws: () => {
        return exports.thrower();
      },
      exn: exports.exn,
    },
  };

  // This passes the exception tag check and the exception is caught.
  assertEq(
    wasmEvalText(
      `(module
         (type (func (param i32)))
         (import "store" "throws" (func $thrower (result i32)))
         (import "store" "exn" (tag $exn (type 0)))
         (func (export "catches") (result i32)
           try (result i32)
             (call $thrower)
           catch $exn
             (i32.const 15)
             (i32.sub)
           end))`,
      imports
    ).exports.catches(),
    27
  );

  // This fails the exception tag check, despite the local exception having
  // a matching signature.
  assertWasmThrowsExn(() =>
    wasmEvalText(
      `(module
         (type (func (param i32)))
         (import "store" "throws" (func $thrower (result i32)))
         (tag $exn (type 0))
         (func (export "catchesFail") (result i32)
           try (result i32)
             (call $thrower)
           catch $exn ;; This should not recognise $exn, thus not unpack 42.
           end))`,
      imports
    ).exports.catchesFail()
  );
}

// Test that JS finally block executes after a Wasm throw.
assertEq(
  (() => {
    try {
      wasmEvalText(
        `(module
           (type (func (param)))
           (tag $exn (type 0))
           (func (export "f")
             (throw $exn)))`
      ).exports.f();
    } finally {
      return true;
    }
    return false;
  })(),
  true
);

// Test that a wasm trap that passes through JS cannot be caught in Wasm.
{
  let throwTrap = wasmEvalText(`(module (func (export "f") unreachable))`)
    .exports.f;
  let catcher = wasmEvalText(
    `(module
       (type (func))
       (tag $exn (type 0))
       (import "m" "f" (func $foreign (param) (result)))
       (func (export "f")
         try
           call $foreign
         catch $exn
         catch_all
         end))`,
    {
      m: {
        // JS frame that goes between the two wasm frames and just rethrows.
        f: () => {
          try {
            throwTrap();
          } catch (e) {
            throw e;
          }
        },
      },
    }
  ).exports.f;

  assertErrorMessage(
    () => catcher(),
    WebAssembly.RuntimeError,
    "unreachable executed"
  );
}

// Test delegate throwing out of function.
assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         try (result i32)
           throw $exn
         delegate 0))`
  ).exports.f()
);

assertWasmThrowsExn(() =>
  wasmEvalText(
    `(module
       (tag $exn (param))
       (func (export "f") (result i32)
         try (result i32)
           i32.const 0
           if
             i32.const 1
             return
           else
             throw $exn
           end
           i32.const 0
         delegate 0))`
  ).exports.f()
);
