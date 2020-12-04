// Tests for Wasm exception import and export.

// The WebAssembly.Exception constructor cannot be called for now until the
// JS API specifies the behavior.
function testException() {
  assertErrorMessage(
    () => new WebAssembly.Exception(),
    WebAssembly.RuntimeError,
    /cannot call WebAssembly.Exception/
  );
}

testException();
