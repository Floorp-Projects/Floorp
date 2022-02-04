// |jit-test| skip-if:true;

// This file contains all the code that is common to all integer gemm intrinsics' test scripts.

// The test setup as string that is common to all integer gemm tests
export const COMMON_TEST_SETUP_AS_STRING = `
const libdir=${JSON.stringify(libdir)}; load(libdir + "wasm.js");
let memory = new WebAssembly.Memory({initial: 1, maximum: 1});
let module = WebAssembly.mozIntGemm();
if (!module) {
  throw new Error();
}
let instance = new WebAssembly.Instance(module, {"": {"memory": memory}});

const ARRAY_ALIGNMENT = 64;
const ROWS_A_MULTIPLIER = 1;
const COLUMNS_A_MULTIPLIER = 64;
const ROWS_B_MULTIPLIER = COLUMNS_A_MULTIPLIER;
const COLUMNS_B_MULTIPLIER = 8;
const SELECTED_COLUMNS_B_MULTIPLIER = 8;
`

// Run the test
export function runTest(completeTestAsString) {
  const testEnvironment = newGlobal({newCompartment: true, systemPrincipal: true});
  testEnvironment.evaluate(completeTestAsString);
}
