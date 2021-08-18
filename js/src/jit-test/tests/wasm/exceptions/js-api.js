// Tests for wasm exception proposal JS API features.

load(libdir + "eqArrayHelper.js");

// WebAssembly.Tag tests.
assertErrorMessage(
  () => WebAssembly.Tag(),
  TypeError,
  /calling a builtin Tag constructor without new is forbidden/
);

assertErrorMessage(
  () => new WebAssembly.Tag(),
  TypeError,
  /At least 1 argument required/
);

assertErrorMessage(
  () => new WebAssembly.Tag(3),
  TypeError,
  /first argument must be a tag descriptor/
);

assertErrorMessage(
  () => new WebAssembly.Tag({ parameters: ["foobar"] }),
  TypeError,
  /bad value type/
);

new WebAssembly.Tag({ parameters: [] });
new WebAssembly.Tag({ parameters: ["i32"] });
new WebAssembly.Tag({ parameters: ["i32", "externref"] });

wasmEvalText(`(module (import "m" "e" (tag)))`, {
  m: { e: new WebAssembly.Tag({ parameters: [] }) },
});

wasmEvalText(`(module (import "m" "e" (tag (param i32))))`, {
  m: { e: new WebAssembly.Tag({ parameters: ["i32"] }) },
});

wasmEvalText(`(module (import "m" "e" (tag (param i32 i64))))`, {
  m: { e: new WebAssembly.Tag({ parameters: ["i32", "i64"] }) },
});

assertErrorMessage(
  () =>
    wasmEvalText(`(module (import "m" "e" (tag (param i32))))`, {
      m: { e: new WebAssembly.Tag({ parameters: [] }) },
    }),
  WebAssembly.LinkError,
  /imported tag 'm.e' signature mismatch/
);

assertErrorMessage(
  () =>
    wasmEvalText(`(module (import "m" "e" (tag (param))))`, {
      m: { e: new WebAssembly.Tag({ parameters: ["i32"] }) },
    }),
  WebAssembly.LinkError,
  /imported tag 'm.e' signature mismatch/
);

// Test WebAssembly.Tag methods.
{
  let params = [
    [],
    ["i32"],
    ["i32", "i64"],
    ["f32", "externref"],
    ["i32", "i64", "f32", "f64"],
  ];

  for (const arg of params) {
    const tag = new WebAssembly.Tag({ parameters: arg });
    assertEqArray(tag.type().parameters, arg);
  }
}

// WebAssembly.Exception tests.
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
     (tag (export "tag1") (param))
     (tag (export "tag2") (param i32))
     (tag (export "tag3") (param i32 f32))
     (tag (export "tag4") (param i32 externref i32))
     (tag (export "tag5") (param i32 externref i32 externref))
     (tag (export "tag6") (param funcref))
     (tag (export "tag7") (param i64)))`
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

// Test Exception methods.
{
  const exn1 = new WebAssembly.Exception(tag1, []);
  assertEq(exn1.is(tag1), true);
  assertEq(exn1.is(tag2), false);
  assertErrorMessage(
    () => exn1.is(),
    TypeError,
    /At least 1 argument required/
  );
  assertErrorMessage(
    () => exn1.is(5),
    TypeError,
    /first argument must be a WebAssembly.Tag/
  );

  const exn2 = new WebAssembly.Exception(tag2, [3]);
  assertEq(exn2.getArg(tag2, 0), 3);

  assertEq(
    new WebAssembly.Exception(tag2, [undefined]).getArg(tag2, 0),
    0
  );

  const exn4 = new WebAssembly.Exception(tag4, [3, "foo", 4]);
  assertEq(exn4.getArg(tag4, 0), 3);
  assertEq(exn4.getArg(tag4, 1), "foo");
  assertEq(exn4.getArg(tag4, 2), 4);

  const exn5 = new WebAssembly.Exception(
    tag5,
    [3,
    "foo",
    4,
    "bar"]
  );
  assertEq(exn5.getArg(tag5, 3), "bar");

  assertErrorMessage(
    () => exn2.getArg(),
    TypeError,
    /At least 2 arguments required/
  );
  assertErrorMessage(
    () => exn2.getArg(5, 0),
    TypeError,
    /first argument must be a WebAssembly.Tag/
  );
  assertErrorMessage(
    () => exn2.getArg(tag2, "foo"),
    TypeError,
    /bad Exception getArg index/
  );
  assertErrorMessage(
    () => exn2.getArg(tag2, 10),
    RangeError,
    /bad Exception getArg index/
  );
}

// Test throwing a JS constructed exception to Wasm.
assertEq(
  wasmEvalText(
    `(module
       (import "m" "exn" (tag $exn (param i32)))
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
       (import "m" "exn" (tag $exn (param i32 f32)))
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
       (import "m" "exn" (tag $exn (param i32 externref i32)))
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
       (import "m" "exn" (tag $exn))
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

{
  const exn = new WebAssembly.Tag({ parameters: ["i32"] });
  assertEq(
    wasmEvalText(
      `(module
         (import "m" "exn" (tag $exn (param i32)))
         (import "m" "f" (func $f))
         (func (export "f") (result i32)
           try (result i32)
             call $f
             (i32.const 0)
           catch $exn
           end))`,
      {
        m: {
          exn: exn,
          f: () => {
            throw new WebAssembly.Exception(exn, [42]);
          },
        },
      }
    ).exports.f(),
    42
  );
}

{
  const exn1 = new WebAssembly.Tag({ parameters: ["i32"] });
  const exn2 = new WebAssembly.Tag({ parameters: ["i32"] });
  assertEq(
    wasmEvalText(
      `(module
         (import "m" "exn" (tag $exn (param i32)))
         (import "m" "f" (func $f))
         (func (export "f") (result i32)
           try (result i32)
             call $f
             (i32.const 0)
           catch $exn
           catch_all
             (i32.const 1)
           end))`,
      {
        m: {
          exn: exn1,
          f: () => {
            throw new WebAssembly.Exception(exn2, [42]);
          },
        },
      }
    ).exports.f(),
    1
  );
}
