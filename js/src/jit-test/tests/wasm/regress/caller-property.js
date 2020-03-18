const { g } = wasmEvalText(`(module (func $f) (export "g" (func $f)))`).exports;

function testCaller() {
  return g.caller;
}

assertErrorMessage(testCaller, TypeError, /caller/);
