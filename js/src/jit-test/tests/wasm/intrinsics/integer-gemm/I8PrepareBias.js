// This file contains all the tests for int8_prepare_bias intrinsic. It depends
// on the CommonTestSetup.js script which contains the common functionality
// that is required for testing all the intrinsics.
const COMMON_TEST_SETUP_SCRIPT = "./CommonTestSetup.js"

// All tests for this intrinsic as a string
const ALL_TESTS_AS_STRING =`
let {int8_prepare_bias} = instance.exports;

const VALID = {input: 0, scaleA: 1.0, zeroPointA: 0.0, scaleB: 1.0, zeroPointB: 0.0, rows: ROWS_B_MULTIPLIER, cols: COLUMNS_B_MULTIPLIER, inputBias: ARRAY_ALIGNMENT << 4, output: ARRAY_ALIGNMENT << 5};

function testInvalidSize() {
  let invalidSize;

  // row: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_prepare_bias(VALID.input, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, invalidSize, VALID.cols, VALID.inputBias, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // row: Not an integral multiple of ROWS_B_MULTIPLIER
  invalidSize = ROWS_B_MULTIPLIER + 1;
  assertErrorMessage(() => int8_prepare_bias(VALID.input, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, invalidSize, VALID.cols, VALID.inputBias, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // col: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_prepare_bias(VALID.input, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, VALID.rows, invalidSize, VALID.inputBias, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // col: Not an integral multiple of COLUMNS_B_MULTIPLIER
  invalidSize = COLUMNS_B_MULTIPLIER + 1;
  assertErrorMessage(() => int8_prepare_bias(VALID.input, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, VALID.rows, invalidSize, VALID.inputBias, VALID.output), WebAssembly.RuntimeError, /unreachable/);
}

function testInvalidAlignment() {
  let invalidAlignment = ARRAY_ALIGNMENT + 1;

  // input: Not an integral multiple of ARRAY_ALIGNMENT
  assertErrorMessage(() => int8_prepare_bias(invalidAlignment, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, VALID.rows, VALID.cols, VALID.inputBias, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);
}

function testOutOfBounds() {
  let outOfBound;

  // input: Out of Bounds
  outOfBound = PageSizeInBytes - ARRAY_ALIGNMENT;
  assertErrorMessage(() => int8_prepare_bias(outOfBound, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, VALID.rows, VALID.cols, VALID.inputBias, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // inputBias: Out of Bounds
  outOfBound = PageSizeInBytes - VALID.cols;
  assertErrorMessage(() => int8_prepare_bias(VALID.input, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, VALID.rows, VALID.cols, outOfBound, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // output: Out of Bounds
  outOfBound = PageSizeInBytes - VALID.cols;
  assertErrorMessage(() => int8_prepare_bias(VALID.input, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, VALID.rows, VALID.cols, VALID.inputBias, outOfBound), WebAssembly.RuntimeError, /index out of bounds/);
}

function testSuccessfulCall() {
  // We just test that with valid arguments the intrinsic executes without any error
  int8_prepare_bias(VALID.input, VALID.scaleA, VALID.zeroPointA, VALID.scaleB, VALID.zeroPointB, VALID.rows, VALID.cols, VALID.inputBias, VALID.output);
}

testInvalidSize();
testInvalidAlignment();
testOutOfBounds();
testSuccessfulCall();
`

// Run all the tests
import(COMMON_TEST_SETUP_SCRIPT).then((importedModule) => {
  importedModule.runTest(importedModule.COMMON_TEST_SETUP_AS_STRING + ALL_TESTS_AS_STRING);
});
