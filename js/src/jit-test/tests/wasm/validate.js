load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

// Note to sheriffs: when merging inbound to aurora, if you get a merging issue
// here, you can safely remove this code and replace it by the inbound code.

// Enable warning as errors.
options("werror");
assertErrorMessage(() => WebAssembly.validate(wasmTextToBinary(`(module (func) (func) (export "a" 2))`, 'new-format')),
                  TypeError,
                  /exported function index out of bounds/);
options("werror");
