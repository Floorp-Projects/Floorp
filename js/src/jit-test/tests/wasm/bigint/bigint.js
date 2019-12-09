// Used to ensure tests will trigger Wasm Jit stub code.
var threshold = 2 * getJitCompilerOptions()["ion.warmup.trigger"] + 10;
function testWithJit(f) {
  for (var i = 0; i < threshold; i++) {
    f();
  }
}

function testRet() {
  var f = wasmEvalText(`(module
    (func (export "f") (result i64) (i64.const 66))
  )`).exports.f;

  testWithJit(() => {
    assertEq(typeof f(), "bigint", "should return a bigint");
    assertEq(f(), 66n, "should return the correct value");
  });
}

function testId() {
  var exports = wasmEvalText(`(module
    (func (export "f") (param i64 i64) (result i64)
      (local.get 0)
    )
    (func (export "f2") (param i64 i64) (result i64)
      (local.get 1)
    )
  )`).exports;
  var f = exports.f;
  var f2 = exports.f2;

  testWithJit(() => {
    assertEq(f(0n, 1n), 0n);
    assertEq(f(-0n, 1n), -0n);
    assertEq(f(123n, 1n), 123n);
    assertEq(f(-123n, 1n), -123n);
    assertEq(f(2n ** 63n, 1n), -(2n ** 63n));
    assertEq(f(2n ** 64n + 123n, 1n), 123n);
    assertEq(f("5", 1n), 5n);

    assertEq(f2(1n, 0n), 0n);
    assertEq(f2(1n, -0n), -0n);
    assertEq(f2(1n, 123n), 123n);
    assertEq(f2(1n, -123n), -123n);
    assertEq(f2(1n, 2n ** 63n), -(2n ** 63n));
    assertEq(f2(1n, 2n ** 64n + 123n), 123n);
    assertEq(f2(1n, "5"), 5n);

    assertErrorMessage(() => f(5, 1n), TypeError, "not a BigInt");
    assertErrorMessage(() => f2(1n, 5), TypeError, "not a BigInt");
  });
}

function testIdPlus() {
  var f = wasmEvalText(`(module
    (func (export "f") (param i64) (result i64)
      (i64.const 8)
      (get_local 0)
      (i64.add)
    )
  )`).exports.f;

  testWithJit(() => {
    assertEq(f(0n), 0n + 8n);
    assertEq(f(147n), 147n + 8n);
  });
}

// Test functions with many parameters to stress ABI cases.
function testManyArgs() {
  var f = wasmEvalText(`(module
    (func (export "f")
      (param i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64)
      (result i64)
      (get_local 0)
      (get_local 1)
      (get_local 2)
      (get_local 3)
      (get_local 4)
      (get_local 5)
      (get_local 6)
      (get_local 7)
      (get_local 8)
      (get_local 9)
      (get_local 10)
      (get_local 11)
      (get_local 12)
      (get_local 13)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
      (i64.add)
    )
  )`).exports.f;

  testWithJit(() => {
    assertEq(f(1n, 1n, 1n, 1n, 1n, 1n, 1n, 1n, 1n, 1n, 1n, 1n, 1n, 1n), 14n);
  });
}

// Test import and re-export.
function testImportExport() {
  var f1 = wasmEvalText(
    `(module
      (import "i64" "func" (param i64))
      (export "f" 0)
    )`,
    {
      i64: {
        func(b) {
          assertEq(b, 42n);
        },
      },
    }
  ).exports.f;

  var f2 = wasmEvalText(
    `(module
      (import "i64" "func" (param i64) (result i64))
      (export "f" 0)
    )`,
    {
      i64: {
        func(n) {
          return n + 1n;
        },
      },
    }
  ).exports.f;

  var f3 = wasmEvalText(
    `(module
      (import $i64 "" "i64" (param i64) (result i64))
      (func (export "f") (param i64) (result i64)
        (get_local 0)
        (call $i64))
    )`,
    {
      "": {
        i64: n => {
          return n + 1n;
        },
      },
    }
  ).exports.f;

  var f4 = wasmEvalText(
    `(module
      (import "i64" "func" (result i64))
      (export "f" 0)
    )`,
    { i64: { func() {} } }
  ).exports.f;

  testWithJit(() => {
    assertEq(f1(42n), undefined);
    assertEq(f2(42n), 43n);
    assertEq(f3(42n), 43n);
    assertErrorMessage(() => f4(42), TypeError, "not a BigInt");
  });
}

// Test that a mixture of I64 and other argument types works.
function testMixedArgs() {
  var f = wasmEvalText(`(module
    (func (export "f")
      (param i64 f32 f64 i32 i64)
      (result i64)
      (get_local 1)
      (i64.trunc_s/f32)
      (get_local 2)
      (i64.trunc_s/f64)
      (i64.add)
      (get_local 3)
      (i64.extend_i32_u)
      (i64.add)
      (get_local 0)
      (i64.add)
      (get_local 4)
      (i64.add)
    )
  )`).exports.f;

  testWithJit(() => {
    assertEq(f(1n, 1.3, 1.7, 1, 1n), 5n);
  });
}

testRet();
testId();
testIdPlus();
testManyArgs();
testImportExport();
testMixedArgs();
