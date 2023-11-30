// This file contains all the tests for int8_multiply_and_add_bias intrinsic. It depends
// on the CommonTestSetup.js script which contains the common functionality
// that is required for testing all the intrinsics.
const COMMON_TEST_SETUP_SCRIPT = "./CommonTestSetup.js"

// All tests for this intrinsic as a string
const ALL_TESTS_AS_STRING =`
let {int8_multiply_and_add_bias} = instance.exports;

const VALID = {inputA: 0, scaleA: 1.0, zeroPointA: 0.0, inputB: ARRAY_ALIGNMENT << 2, scaleB: 1.0, zeroPointB: 0.0, inputBiasPrepared: ARRAY_ALIGNMENT << 4, unquantMultiplier: -2.0, rowsA: ROWS_A_MULTIPLIER, width: ROWS_B_MULTIPLIER, colsB: COLUMNS_B_MULTIPLIER, output: ARRAY_ALIGNMENT << 5};

function testInvalidSize() {
  let invalidSize;

  // rowsA: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, invalidSize, VALID.width, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // width: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, invalidSize, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // width: Not an integral multiple of ROWS_B_MULTIPLIER
  invalidSize = ROWS_B_MULTIPLIER + 1;
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, invalidSize, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // colB: 0
  invalidSize = 0;
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, VALID.width, invalidSize, VALID.output), WebAssembly.RuntimeError, /unreachable/);

  // colB: Not an integral multiple of COLUMNS_B_MULTIPLIER
  invalidSize = COLUMNS_B_MULTIPLIER + 1;
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, VALID.width, invalidSize, VALID.output), WebAssembly.RuntimeError, /unreachable/);
}

function testInvalidAlignment() {
  let invalidAlignment = ARRAY_ALIGNMENT + 1;

  // inputA: Not an integral multiple of ARRAY_ALIGNMENT
  assertErrorMessage(() => int8_multiply_and_add_bias(invalidAlignment, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, VALID.width, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // inputB: Not an integral multiple of ARRAY_ALIGNMENT
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, invalidAlignment, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, VALID.width, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);
}

function testOutOfBounds() {
  let outOfBound;

  // inputA: Out of Bounds
  outOfBound = PageSizeInBytes - ARRAY_ALIGNMENT;
  let rowsAForOutOfBound = ROWS_A_MULTIPLIER << 1;
  assertErrorMessage(() => int8_multiply_and_add_bias(outOfBound, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, rowsAForOutOfBound, VALID.width, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // inputB: Out of Bounds
  outOfBound = PageSizeInBytes - ARRAY_ALIGNMENT;
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, outOfBound, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, VALID.width, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // inputBias: Out of Bounds
  outOfBound = PageSizeInBytes - VALID.colsB;
  assertErrorMessage(() => int8_multiply_and_add_bias(outOfBound, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, outOfBound, VALID.unquantMultiplier, VALID.rowsA, VALID.width, VALID.colsB, VALID.output), WebAssembly.RuntimeError, /index out of bounds/);

  // output: Out of Bounds
  outOfBound = PageSizeInBytes - VALID.colsB;
  assertErrorMessage(() => int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, VALID.width, VALID.colsB, outOfBound), WebAssembly.RuntimeError, /index out of bounds/);
}

function testSuccessfulCall() {
  // We just test that with valid arguments the intrinsic executes without any error
  int8_multiply_and_add_bias(VALID.inputA, VALID.scaleA, VALID.zeroPointA, VALID.inputB, VALID.scaleB, VALID.zeroPointB, VALID.inputBiasPrepared, VALID.unquantMultiplier, VALID.rowsA, VALID.width, VALID.colsB, VALID.output);
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
