
function expectRunFailure(text, pattern, imports) {
    let instance = wasmEvalText(text, imports);
    assertErrorMessage(() => instance.exports.run(),
		       TypeError,
                       pattern);
}

expectRunFailure(`
  (module
    (import "env" "f" (func $f (result i32 i32)))
    (func (export "run") (result i32)
      (call $f)
      i32.sub))`,
                 /calling JavaScript functions with multiple results from WebAssembly not yet implemented/,
                 { env: { f: () => [52, 10] } });

expectRunFailure(`
  (module
    (func (export "run") (result i32 i32)
      (i32.const 52)
      (i32.const 10)))`,
                 /calling WebAssembly functions with multiple results from JavaScript not yet implemented/);
