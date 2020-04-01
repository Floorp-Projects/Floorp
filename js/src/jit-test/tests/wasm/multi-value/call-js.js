
wasmFullPass(`
  (module
    (import "env" "f" (func $f (result i32 i32 i32)))
    (func (export "run") (result i32)
      (call $f)
      i32.sub
      i32.sub))`,
             42,
             { env: { f: () => [52, 10, 0] } });

if (wasmBigIntEnabled()) {
    wasmFullPass(`
      (module
        (import "env" "f" (func $f (result i64 i64 i64)))
        (func (export "run") (result i64)
          (call $f)
          i64.sub
          i64.sub))`,
                 42n,
                 { env: { f: () => [52n, 10n, 0n] } });
}

wasmFullPass(`
  (module
    (import "env" "f" (func $f (result f32 f32 f32)))
    (func (export "run") (result i32)
      (call $f)
      f32.sub
      f32.sub
      i32.trunc_f32_s))`,
             42,
             { env: { f: () => [52.25, 10.5, 0.25] } });

wasmFullPass(`
  (module
    (import "env" "f" (func $f (result f64 f64 f64)))
    (func (export "run") (result i32)
      (call $f)
      f64.sub
      f64.sub
      i32.trunc_f64_s))`,
             42,
             { env: { f: () => [52.25, 10.5, 0.25] } });

// Multiple values are returned as an iterable; it doesn't have to be an array.
function expectMultiValuePass(f) {
    wasmFullPass(`
      (module
        (import "env" "f" (func $f (result i32 i32 i32)))
        (func (export "run") (result i32)
          (call $f)
          i32.sub
          i32.sub))`,
                 42,
                 { env: { f } });
}
function expectMultiValueError(f, type, pattern) {
    let module = new WebAssembly.Module(wasmTextToBinary(`
      (module
        (import "env" "f" (func $f (result i32 i32 i32)))
        (func (export "run") (result i32)
          (call $f)
          i32.sub
          i32.sub))`));

    let instance = new WebAssembly.Instance(module, { env: { f } } );
    assertErrorMessage(() => instance.exports.run(), type, pattern);
}

expectMultiValuePass(() => [52, 10, 0]);
expectMultiValuePass(() => [32, -10, 0]);
expectMultiValuePass(() => [52.75, 10, 0.5]); // Values converted to i32 via ToInt32.
expectMultiValuePass(() => [42, undefined, undefined]);
expectMultiValuePass(() => (function*() { yield 52; yield 10; yield 0; })());
expectMultiValuePass(() => (function*() { yield '52'; yield '10'; yield 0; })());

// Multi-value result must be iterable.
expectMultiValueError(() => 1, TypeError, /iterable/);
expectMultiValueError(() => 1n, TypeError, /iterable/);

// Check that the iterator's values are collected first, and that the
// length of the result is correct.
expectMultiValueError(() => [1], TypeError, /expected 3, got 1/);
expectMultiValueError(() => [1n], TypeError, /expected 3, got 1/);
expectMultiValueError(() => [52, 10, 0, 0], TypeError, /expected 3, got 4/);
expectMultiValueError(() => (function*() { yield 52; yield 10; yield 0; yield 0; })(),
                      TypeError, /expected 3, got 4/);

// Check that side effects from conversions are done in order.
{
    let calls = [];
    function log(x) { calls.push(x); return x; }
    function logged(x) { return { valueOf: () => log(x) } }
    expectMultiValuePass(() => [logged(52), logged(10), logged(0)]);
    assertEq(calls.join(','), '52,10,0');
}

function expectRunFailure(text, pattern, imports) {
    let instance = wasmEvalText(text, imports);
    assertErrorMessage(() => instance.exports.run(),
		       TypeError,
                       pattern);
}

expectRunFailure(`
  (module
    (func (export "run") (result i32 i32)
      (i32.const 52)
      (i32.const 10)))`,
                 /calling WebAssembly functions with multiple results from JavaScript not yet implemented/);
