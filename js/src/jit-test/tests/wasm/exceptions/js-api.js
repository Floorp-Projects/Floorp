// Tests for wasm exception proposal JS API features.

load(libdir + "eqArrayHelper.js");

assertErrorMessage(
  () => WebAssembly.Exception(),
  TypeError,
  /calling a builtin Exception constructor without new is forbidden/
);

assertErrorMessage(
  () => new WebAssembly.Exception(),
  TypeError,
  /At least 2 arguments required/
);

assertErrorMessage(
  () => new WebAssembly.Exception(3, []),
  TypeError,
  /first argument must be a WebAssembly.Tag/
);

const { tag1, tag2, tag3, tag4, tag5, tag6, tag7 } = wasmEvalText(
  `(module
     (event (export "tag1") (param))
     (event (export "tag2") (param i32))
     (event (export "tag3") (param i32 f32))
     (event (export "tag4") (param i32 externref i32))
     (event (export "tag5") (param i32 externref i32 externref))
     (event (export "tag6") (param funcref))
     (event (export "tag7") (param i64)))`
).exports;

new WebAssembly.Exception(tag1, []);
new WebAssembly.Exception(tag2, [3]);
new WebAssembly.Exception(tag3, [3, 5.5]);
new WebAssembly.Exception(tag4, [3, "foo", 4]);
new WebAssembly.Exception(tag5, [3, "foo", 4, "bar"]);

assertErrorMessage(
  () => new WebAssembly.Exception(tag2, []),
  TypeError,
  /expected 1 values but got 0/
);

assertErrorMessage(
  () => new WebAssembly.Exception(tag2, [3n]),
  TypeError,
  /can't convert BigInt to number/
);

assertErrorMessage(
  () => new WebAssembly.Exception(tag6, [undefined]),
  TypeError,
  /can only pass WebAssembly exported functions to funcref/
);

assertErrorMessage(
  () => new WebAssembly.Exception(tag7, [undefined]),
  TypeError,
  /can't convert undefined to BigInt/
);

assertErrorMessage(
  () => new WebAssembly.Exception(tag7, {}),
  TypeError,
  /\({}\) is not iterable/
);

assertErrorMessage(
  () => new WebAssembly.Exception(tag7, 1),
  TypeError,
  /second argument must be an object/
);

// Test throwing a JS constructed exception to Wasm.
assertEq(
  wasmEvalText(
    `(module
       (import "m" "exn" (event $exn (param i32)))
       (import "m" "f" (func $f))
       (func (export "f") (result i32)
         try (result i32)
           call $f
           (i32.const 0)
         catch $exn
         end))`,
    {
      m: {
        exn: tag2,
        f: () => {
          throw new WebAssembly.Exception(tag2, [42]);
        },
      },
    }
  ).exports.f(),
  42
);

assertEqArray(
  wasmEvalText(
    `(module
       (import "m" "exn" (event $exn (param i32 f32)))
       (import "m" "f" (func $f))
       (func (export "f") (result i32 f32)
         try (result i32 f32)
           call $f
           (i32.const 0)
           (f32.const 0)
         catch $exn
         end))`,
    {
      m: {
        exn: tag3,
        f: () => {
          throw new WebAssembly.Exception(tag3, [42, 5.5]);
        },
      },
    }
  ).exports.f(),
  [42, 5.5]
);

assertEqArray(
  wasmEvalText(
    `(module
       (import "m" "exn" (event $exn (param i32 externref i32)))
       (import "m" "f" (func $f))
       (func (export "f") (result i32 externref i32)
         try (result i32 externref i32)
           call $f
           (i32.const 0)
           (ref.null extern)
           (i32.const 0)
         catch $exn
         end))`,
    {
      m: {
        exn: tag4,
        f: () => {
          throw new WebAssembly.Exception(tag4, [42, "foo", 42]);
        },
      },
    }
  ).exports.f(),
  [42, "foo", 42]
);

assertEq(
  wasmEvalText(
    `(module
       (import "m" "exn" (event $exn))
       (import "m" "f" (func $f))
       (func (export "f") (result i32)
         try (result i32)
           call $f
           (i32.const 0)
         catch $exn
           (i32.const 0)
         catch_all
           (i32.const 1)
         end))`,
    {
      m: {
        exn: tag1,
        f: () => {
          throw new WebAssembly.Exception(tag2, [42]);
        },
      },
    }
  ).exports.f(),
  1
);
