// Test for bug 1633740, an intermittent GC-related crash caused by
// the bigint/i64 conversion in inlined Ion to Wasm calls.

// Used to help ensure this will trigger the Ion inlined call path.
var threshold = 2 * getJitCompilerOptions()["ion.warmup.trigger"] + 10;
function testWithJit(f) {
  for (var i = 0; i < threshold; i++) {
    f();
  }
}

function test() {
  var exports = wasmEvalText(`(module
    (func (export "f") (param i64) (result i64)
      (local.get 0)
    ))`).exports;
  var f = exports.f;

  testWithJit(() => {
    assertEq(f("5"), 5n);
  });
}

gczeal(7, 1); // Collect nursery on every allocation.
test();
